using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using ParkingDashboard.Services;
using ParkingDashboard.ViewModels;

namespace ParkingDashboard.Controllers;

/// <summary>
/// LLM 기반 데이터 조회 (관리자 전용)
/// - 자연어 질문을 분석해서 데이터베이스 조회 결과를 자연스러운 한국어로 반환.
/// </summary>
[Authorize]
public class ReportsController : Controller
{
    private readonly ILLMService _llm;

    public ReportsController(ILLMService llm) => _llm = llm;

    [HttpGet]
    public IActionResult Index() => View(new ReportViewModel
    {
        Answer = "좌측 제안 질문을 클릭하거나 직접 질문을 입력하세요."
    });

    [HttpPost]
    [ValidateAntiForgeryToken]
    public async Task<IActionResult> Ask(string question, CancellationToken ct)
    {
        if (string.IsNullOrWhiteSpace(question))
            return RedirectToAction(nameof(Index));

        var result = await _llm.AskAsync(question, ct);
        return View("Index", result);
    }
}
