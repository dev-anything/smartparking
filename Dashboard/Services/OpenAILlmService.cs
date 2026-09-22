using System.ClientModel;
using Microsoft.EntityFrameworkCore;
using OpenAI;
using OpenAI.Chat;
using ParkingDashboard.Data;
using ParkingDashboard.ViewModels;

namespace ParkingDashboard.Services;

/// <summary>
/// OpenAI 호환 LLM 서비스 (옵션)
/// - appsettings.json 의 Llm:OpenApiKey 가 비어 있으면 사용할 수 없습니다.
/// </summary>
public class OpenAILlmService : ILLMService
{
    private readonly ApplicationDbContext _db;
    private readonly IConfiguration _cfg;
    private readonly ILogger<OpenAILlmService> _logger;

    public OpenAILlmService(ApplicationDbContext db, IConfiguration cfg, ILogger<OpenAILlmService> logger)
    {
        _db = db;
        _cfg = cfg;
        _logger = logger;
    }

    public async Task<ReportViewModel> AskAsync(string question, CancellationToken ct = default)
    {
        var vm = new ReportViewModel { Question = question, AskedAt = DateTime.Now };
        var apiKey = _cfg["Llm:OpenApiKey"];
        var model = _cfg["Llm:OpenModel"] ?? "gpt-4o-mini";

        if (string.IsNullOrWhiteSpace(apiKey))
        {
            vm.Answer = "OpenAI API 키가 설정되지 않았습니다. (appsettings.json → Llm.OpenApiKey)";
            return vm;
        }

        ct.ThrowIfCancellationRequested();
        try
        {
            var client = new ChatClient(model, new ApiKeyCredential(apiKey));
            var systemPrompt = _cfg["Llm:SystemPrompt"] ?? "당신은 주차장 데이터 어시스턴트입니다.";
            var userPrompt = $"질문: {question}\n\n(db schema 참고: vehicles, entryexitrecords, fees)";

            var messages = new ChatMessage[]
            {
                new SystemChatMessage(systemPrompt),
                new UserChatMessage(userPrompt)
            };
            var resp = await client.CompleteChatAsync(messages);
            vm.Answer = resp.Value.Content[0].Text;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "OpenAI LLM 호출 실패");
            vm.Answer = "LLM 호출에 실패했습니다: " + ex.Message;
        }

        return vm;
    }

    public async Task<string> AskSqlAsync(string prompt, CancellationToken ct = default)
    {
        var apiKey = _cfg["Llm:OpenApiKey"];
        var model = _cfg["Llm:OpenModel"] ?? "gpt-4o-mini";

        if (string.IsNullOrWhiteSpace(apiKey))
        {
            throw new InvalidOperationException("OpenAI API 키가 설정되지 않았습니다. (appsettings.json → Llm.OpenApiKey)");
        }

        ct.ThrowIfCancellationRequested();
        try
        {
            var client = new ChatClient(model, new ApiKeyCredential(apiKey));
            
            var messages = new ChatMessage[]
            {
                new SystemChatMessage("당신은 SQLite SQL 쿼리 생성 전문가입니다. 오직 SELECT 쿼리만 생성하며, 다른 설명은 절대 추가하지 않습니다."),
                new UserChatMessage(prompt)
            };
            var resp = await client.CompleteChatAsync(messages);
            var rawSql = resp.Value.Content[0].Text.Trim();
            
            // 마크다운 문법 제거 (```sql ... ``` 등)
            if (rawSql.StartsWith("```"))
            {
                var lines = rawSql.Split('\n');
                if (lines.Length > 2)
                    rawSql = string.Join('\n', lines.Skip(1).Take(lines.Length - 2)).Trim();
            }
            
            return rawSql;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "OpenAI LLM SQL 쿼리 생성 실패");
            throw new Exception("LLM SQL 생성 실패: " + ex.Message, ex);
        }
    }
}
