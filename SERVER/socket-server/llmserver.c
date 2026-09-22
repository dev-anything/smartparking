#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <microhttpd.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <mysql/mysql.h>

#define SERVER_PORT 10002
#define LLM_API_URL "http://localhost:10002/v1/chat/completions"
#define MYSQL_HOST "127.0.0.1"
#define MYSQL_USER "server"
#define MYSQL_PASSWORD "server"
#define MYSQL_DB "smartparking"
#define MYSQL_TABLE "records"

static const char *SCHEMA =
    "TABLE records (id INT PK NOT NULL, car_number CHAR(30) NOT NULL, entry_time DATETIME NOT NULL, exit_time DATETIME NULL)\n"
    "각 필드 정보:\n"
    "id: 자동 증가하는 기본키\n"
    "car_number: 차량번호\n"
    "entry_time: 주차장 입차 시각\n"
    "exit_time: 주차장 출차 시각(미출차시 NULL)\n";

static const char *FEWSHOT_EXAMPLES =
    //"질문: 아직 출차하지 않은 차량번호를 알려줘\n"
    "SELECT * FROM records;\n"
    "SELECT car_number FROM records WHERE exit_time IS NULL;\n\n";

struct membuf
{
    char *data;
    size_t len;
};

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata);
static char *call_llm(const char *user_prompt);

int main()
{
    MYSQL *conn;
    MYSQL_RES *res;
    MYSQL_ROW rows;
    int query_response;
    char question[2048];
    char prompt[2048];

    conn = mysql_init(NULL);
    curl_global_init(CURL_GLOBAL_ALL);

    // 연결 확인
    if (!(mysql_real_connect(conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWORD, MYSQL_DB, 3306, NULL, 0)))
    {
        fprintf(stderr, "err: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
        exit(1);
    }

    printf("MySQL connected !\n");
    printf("궁금한 사항을 질문하세요: ");
    fgets(question, sizeof(question), stdin);

    //sprintf(
    //    question,
    //    "2026년 9월 18일에 출차한 차량들의 차량번호를 알려줘."
    //    //"%s 테이블에 데이터가 몇 개 있는지 알려줘",
    //    // MYSQL_TABLE
    //);

    sprintf(
        prompt,
        "스키마: %s\n\n"
        "위 스키마만 사용해서 MySQL SELECT 쿼리를 작성해.\n\n"
        "규칙:\n"
        "1. SELECT만 사용. INSERT, UPDATE, DELETE, DROP 등은 절대 사용 금지.\n"
        "2. 스키마에 없는 테이블이나 컬럼은 사용 금지.\n"
        "3. 세미콜론으로 끝낼 것.\n"
        "4. 답은 SQL 문장 그 자체만 출력. 다른 글자, 기호, 줄바꿈도 앞뒤에 절대 붙이지 마.\n"
        "5. 출력 텍스트에 포함된 마크다운 문법은 모두 제거해.\n\n"
        "6. COUNT, SUM 등 집계 함수와 일반 컬럼을 함께 SELECT하지 마.\n"
        "7. 집계 함수만 쓰거나, 일반 컬럼만 쓰는 쿼리를 작성해.\n"
        "8. 입차 키워드는 entry_time 필드를 사용해.\n"
        "9. 출차 키워드는 exit_time 필드를 사용해.\n"
        "예시 출력:\n%s\n"
        "질문: %s\n",
        SCHEMA, FEWSHOT_EXAMPLES, question);

    printf("=== LLM 전송 프롬프트 ===\n%s\n", prompt);

    char *answer = call_llm(prompt);

    printf("=== LLM 응답 (생성된 SQL) ===\n");

    if (answer)
        printf("%s\n", answer);
    else
        printf("(응답을 받지 못했습니다. llama-server가 실행 중인지, LLM_URL이 올바른지 확인하세요.)\n");

    query_response = mysql_query(
        conn,
        answer);

    if (!query_response)
    {
        res = mysql_store_result(conn);

        if (res)
        {
            printf("Retrived %lu rows\n", (unsigned long)mysql_num_rows(res));

            while ((rows = mysql_fetch_row(res)))
            {
                printf("%s\n", rows[0]);
                // printf("%10s %10s %10s %10s\n",
                //     rows[0] ? rows[0] : "NULL",
                //     rows[1] ? rows[1] : "NULL",
                //     rows[2] ? rows[2] : "NULL",
                //     rows[3] ? rows[3] : "NULL"
                //);
            }
        }
    }
    else
    {
        fprintf(stderr, "SELECT error %s[%d]\n", mysql_error(conn), mysql_errno(conn));
    }
    free(answer);
    curl_global_cleanup();
    mysql_free_result(res);
    mysql_close(conn);
    return 0;
}

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t real = size * nmemb;
    struct membuf *mb = (struct membuf *)userdata;

    char *temp = realloc(mb->data, mb->len + real + 1);
    if (!temp)
        return 0;

    mb->data = temp;
    memcpy(mb->data + mb->len, ptr, real);
    mb->len += real;
    mb->data[mb->len] = '\0';

    return real;
}

static char *call_llm(const char *user_prompt)
{
    // *** 1) curl 핸들 생성 ***
    CURL *curl = curl_easy_init();

    // curl 생성 확인
    if (!curl)
    {
        fprintf(stderr, "Cannot initialize curl.\n");
        return NULL;
    }
    // *** 1) curl 핸들 생성 ***

    // *** 2) JSON 만들기 ***
    // 최상위 객체: {}
    struct json_object *root = json_object_new_object();

    // messages 배열: []
    struct json_object *messages = json_object_new_array();

    // messages[0] = {"role": "user", "content": user_prompt}
    struct json_object *msg = json_object_new_object();
    json_object_object_add(msg, "role", json_object_new_string("user"));
    json_object_object_add(msg, "content", json_object_new_string(user_prompt));
    json_object_array_add(messages, msg);

    // 최상위 객체(root)에 정보 결합
    json_object_object_add(root, "messages", messages);
    // json_object_object_add(root, "max_tokens", json_object_new_int(150));
    //  temperature 값이 낮은 이유: 정답이 명확한 텍스트 생성 시에는 무작위성을 줄여야 함
    json_object_object_add(root, "temperature", json_object_new_double(0.1));

    // 전체를 실제 전송할 문자열로 변환
    const char *body = json_object_to_json_string(root);
    // *** 2) JSON 만들기 ***

    // *** 3) 응답 버퍼 준비 ***
    struct membuf mb;
    mb.data = malloc(1);
    mb.data[0] = '\0';
    mb.len = 0;
    // *** 3) 응답 버퍼 준비 ***

    // *** 4) curl 옵션 설정 ***
    // HTTP 헤더
    struct curl_slist *headers = NULL;
    // Content-Type 지정
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // 요청 주소
    curl_easy_setopt(curl, CURLOPT_URL, LLM_API_URL);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    // JSON 바디 , 헤더 결합
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mb);

    // 요청 타임아웃 설정(120초 / 소형 LLM이라도 토큰 생성에 시간이 걸릴 수 있음, 넉넉하게 설정)
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    // *** 4) curl 옵션 설정 ***

    // *** 5) 요청 전송 ***
    CURLcode res = curl_easy_perform(curl);
    // 최종 반환할 답변 텍스트
    char *content = NULL;

    if (res != CURLE_OK)
    {
        fprintf(stderr, "LLM 요청 실패: %s\n", curl_easy_strerror(res));
    }
    else
    {
        // *** 6) 응답 JSON 파싱 ***
        struct json_object *resp = json_tokener_parse(mb.data);

        if (!resp) // mb.data가 JSON 형식이 아닌 경우
        {
            fprintf(stderr, "LLM 응답 JSON 파싱 실패. 원본: \n%s\n", mb.data);
        }
        else
        {
            struct json_object *choices, *first, *message, *content_obj;

            if (json_object_object_get_ex(resp, "choices", &choices) && json_object_array_length(choices) > 0)
            {
                first = json_object_array_get_idx(choices, 0);

                if (json_object_object_get_ex(first, "message", &message) && json_object_object_get_ex(message, "content", &content_obj))
                {
                    content = strdup(json_object_get_string(content_obj));
                }
            }
            else
            {
                fprintf(stderr, "Unexpected response. 원본 응답:\n%s\n", mb.data);
            }

            json_object_put(resp);
        }
        // *** 6) 응답 JSON 파싱 ***
    }
    // *** 5) 요청 전송 ***

    // *** 7) 리소스 정리 ***
    // 헤더 리스트 해제
    curl_slist_free_all(headers);
    // curl 핸들 해제
    curl_easy_cleanup(curl);
    // 요청 JSON 트리 해제
    json_object_put(root);

    free(mb.data);
    // *** 7) 리소스 정리 ***

    return content;
}