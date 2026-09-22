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
}
