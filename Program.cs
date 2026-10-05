using System.Text;
using System.Text.Json;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddRazorComponents()
    .AddInteractiveServerComponents();
builder.Services.AddHttpClient();

var app = builder.Build();

app.UseStaticFiles();
app.UseAntiforgery();

// 토스페이먼츠 빌링키 발급 승인 API
app.MapPost("/api/billing/issue", async (BillingIssueRequest req, HttpClient httpClient, IConfiguration config) =>
{
    if (string.IsNullOrWhiteSpace(req.TossAuthKey) || string.IsNullOrWhiteSpace(req.TossCustomerKey))
    {
        return Results.BadRequest(new { success = false, message = "인증 키 정보가 누락되었습니다." });
    }

    // appsettings.json 또는 환경변수에서 키를 읽어오도록 설정 (기본값 설정)
    string secretKey = config["TossSecretKey"] ?? "test_sk_zXLk50E43117W41E44ad32wP18ae";
    string encodedKey = Convert.ToBase64String(Encoding.UTF8.GetBytes($"{secretKey}:"));
    string tossUrl = "https://api.tosspayments.com/v1/billing/authorizations/issue";

    try
    {
        var requestMessage = new HttpRequestMessage(HttpMethod.Post, tossUrl);
        requestMessage.Headers.Authorization = new System.Net.Http.Headers.AuthenticationHeaderValue("Basic", encodedKey);
        
        var requestData = new { authKey = req.TossAuthKey, customerKey = req.TossCustomerKey };
        requestMessage.Content = new StringContent(JsonSerializer.Serialize(requestData), Encoding.UTF8, "application/json");

        var response = await httpClient.SendAsync(requestMessage);
        string responseBody = await response.Content.ReadAsStringAsync();

        if (response.IsSuccessStatusCode)
        {
            using var doc = JsonDocument.Parse(responseBody);
            string billingKey = doc.RootElement.GetProperty("billingKey").GetString() ?? "";
            return Results.Ok(new { success = true, message = "빌링키 발급 성공", billingKey = billingKey });
        }
        else
        {
            return Results.Json(new { success = false, message = "빌링키 발급 실패", error = responseBody }, statusCode: (int)response.StatusCode);
        }
    }
    catch (Exception ex)
    {
        return Results.Json(new { success = false, message = ex.Message }, statusCode: 500);
    }
});

app.MapRazorComponents<NewDashboard.Components.App>()
    .AddInteractiveServerRenderMode();

app.Run();

record BillingIssueRequest(string TossAuthKey, string TossCustomerKey, string CarNumber);