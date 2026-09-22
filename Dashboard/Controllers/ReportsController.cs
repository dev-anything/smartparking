using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using ParkingDashboard.Services;
using ParkingDashboard.ViewModels;

namespace ParkingDashboard.Controllers;

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
