using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;
using ParkingDashboard.Data;
using ParkingDashboard.Models;

namespace ParkingDashboard.Controllers;

/// <summary>
/// 입출입 기록 조회 + 수동 등록
/// </summary>
[Authorize]
public class EntryExitController : Controller
{
    private readonly ApplicationDbContext _db;

    public EntryExitController(ApplicationDbContext db) => _db = db;

    public async Task<IActionResult> Index(
        string? plate = null,
        DateTime? from = null,
        DateTime? to = null,
        string? status = null,
        int page = 1)
    {
        const int pageSize = 30;
        var query = _db.EntryExitRecords.AsQueryable();

        if (!string.IsNullOrWhiteSpace(plate))
            query = query.Where(e => e.PlateNumber.Contains(plate));

        if (from.HasValue)
            query = query.Where(e => e.EntryTime >= from.Value);

        if (to.HasValue)
        {
            var end = to.Value.Date.AddDays(1);
            query = query.Where(e => e.EntryTime < end);
        }

        if (status == "parked")
            query = query.Where(e => e.ExitTime == null);
        else if (status == "done")
            query = query.Where(e => e.ExitTime != null);

        var totalCount = await query.CountAsync();
        var totalPages = (int)Math.Ceiling(totalCount / (double)pageSize);
        if (page < 1) page = 1;
        if (page > totalPages && totalPages > 0) page = totalPages;

        var items = await query
            .OrderByDescending(e => e.EntryTime)
            .Skip((page - 1) * pageSize)
            .Take(pageSize)
            .ToListAsync();

        ViewBag.Plate = plate;
        ViewBag.From = from;
        ViewBag.To = to;
        ViewBag.Status = status;
        ViewBag.Page = page;
        ViewBag.TotalCount = totalCount;
        ViewBag.TotalPages = totalPages;

        return View(items);
    }

    /// <summary>수동으로 출차 처리 (요금 산정)</summary>
    [HttpPost]
    [ValidateAntiForgeryToken]
    public async Task<IActionResult> ExitNow(int id, decimal amount = 0, string paymentMethod = "계좌이체")
    {
        var record = await _db.EntryExitRecords.FindAsync(id);
        if (record == null) return NotFound();
        if (record.ExitTime != null)
        {
            TempData["Error"] = "이미 출차 처리된 기록입니다.";
            return RedirectToAction(nameof(Index));
        }

        var now = DateTime.UtcNow;
        record.ExitTime = now;

        var minutes = (int)Math.Round((now - record.EntryTime).TotalMinutes);
        var finalAmount = amount > 0 ? amount : CalcFee(minutes);

        var fee = new Fee
        {
            PlateNumber = record.PlateNumber,
            EntryExitRecordId = record.Id,
            EntryTime = record.EntryTime,
            ExitTime = now,
            ParkedMinutes = minutes,
            Amount = finalAmount,
            PaymentMethod = paymentMethod,
            PaidAt = now
        };

        _db.Fees.Add(fee);
        await _db.SaveChangesAsync();

        TempData["Success"] = $"출차 처리 완료 ({record.PlateNumber}, {minutes}분, {finalAmount:N0}원)";
        return RedirectToAction(nameof(Index));
    }

    /// <summary>수동 입차 기록 등록 (테스트/백업용)</summary>
    [HttpPost]
    [ValidateAntiForgeryToken]
    public async Task<IActionResult> ManualEntry(string plateNumber, string? note)
    {
        if (string.IsNullOrWhiteSpace(plateNumber))
        {
            TempData["Error"] = "차량번호를 입력하세요.";
            return RedirectToAction(nameof(Index));
        }

        _db.EntryExitRecords.Add(new EntryExitRecord
        {
            PlateNumber = plateNumber.Trim(),
            EntryTime = DateTime.UtcNow,
            Source = "Manual",
            Note = note
        });
        await _db.SaveChangesAsync();
        TempData["Success"] = "입차 기록이 추가되었습니다.";
        return RedirectToAction(nameof(Index));
    }

    private decimal CalcFee(int minutes)
    {
        var hourlyRate = _db.Database.IsSqlite()
            ? 1000m
            : 1000m; // 기본 시간당 1000원
        if (minutes <= 15) return 0m;
        return (decimal)Math.Ceiling(minutes / 60.0) * hourlyRate;
    }
}
