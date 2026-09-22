using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;
using ParkingDashboard.Data;

namespace ParkingDashboard.Controllers;

/// <summary>
/// 수익 조회 (일 / 주 / 월 / 기간별)
/// </summary>
[Authorize]
public class RevenueController : Controller
{
    private readonly ApplicationDbContext _db;

    public RevenueController(ApplicationDbContext db) => _db = db;

    public async Task<IActionResult> Index(DateTime? from = null, DateTime? to = null)
    {
        var now = DateTime.Now;
        var start = from ?? new DateTime(now.Year, now.Month, 1);
        var endExclusive = (to ?? now).Date.AddDays(1);

        var rows = await _db.Fees
            .Where(f => f.PaidAt >= start && f.PaidAt < endExclusive)
            .OrderByDescending(f => f.PaidAt)
            .ToListAsync();

        var totalAmount = rows.Sum(f => f.Amount);
        var totalCount = rows.Count;
        var avgAmount = totalCount > 0 ? totalAmount / totalCount : 0m;

        // 결제수단별 통계
        var methodGroup = rows
            .GroupBy(r => r.PaymentMethod)
            .Select(g => new { Method = g.Key, Total = g.Sum(x => x.Amount), Count = g.Count() })
            .OrderByDescending(g => g.Total)
            .ToList();

        // 일별 집계 (차트용)
        var rawDaily = rows
            .GroupBy(r => r.PaidAt.Date)
            .Select(g => new { Day = g.Key, Total = g.Sum(x => x.Amount), Count = g.Count() })
            .OrderBy(x => x.Day)
            .ToList();

        ViewBag.From = start;
        ViewBag.To = (to ?? now).Date;
        ViewBag.TotalAmount = totalAmount;
        ViewBag.TotalCount = totalCount;
        ViewBag.AvgAmount = avgAmount;
        ViewBag.MethodGroup = methodGroup;
        ViewBag.Daily = rawDaily;

        return View(rows);
    }
}
