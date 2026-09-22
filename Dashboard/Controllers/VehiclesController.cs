using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;
using ParkingDashboard.Data;
using ParkingDashboard.Models;
using ParkingDashboard.ViewModels;

namespace ParkingDashboard.Controllers;

/// <summary>
/// 차량 등록 / 조회 / 수정 / 삭제 (CRUD)
/// </summary>
[Authorize]
public class VehiclesController : Controller
{
    private readonly ApplicationDbContext _db;

    public VehiclesController(ApplicationDbContext db) => _db = db;

    public async Task<IActionResult> Index(string? search = null)
    {
        var q = _db.Vehicles.AsQueryable();
        if (!string.IsNullOrWhiteSpace(search))
        {
            q = q.Where(v =>
                v.PlateNumber.Contains(search) ||
                v.OwnerName.Contains(search) ||
                v.PhoneNumber.Contains(search));
        }
        var list = await q.OrderByDescending(v => v.RegisteredAt).ToListAsync();
        ViewBag.Search = search;
        return View(list);
    }

    public async Task<IActionResult> Details(string id)
    {
        if (string.IsNullOrEmpty(id)) return BadRequest();
        var vehicle = await _db.Vehicles.FirstOrDefaultAsync(v => v.PlateNumber == id);
        if (vehicle == null) return NotFound();

        var records = await _db.EntryExitRecords
            .Where(e => e.PlateNumber == id)
            .OrderByDescending(e => e.EntryTime)
            .Take(30)
            .ToListAsync();

        var fees = await _db.Fees
            .Where(f => f.PlateNumber == id)
            .OrderByDescending(f => f.PaidAt)
            .Take(30)
            .ToListAsync();

        ViewBag.Records = records;
        ViewBag.Fees = fees;
        return View(vehicle);
    }

    [HttpGet]
    public IActionResult Create() => View(new VehicleViewModel());

    [HttpPost]
    [ValidateAntiForgeryToken]
    public async Task<IActionResult> Create(VehicleViewModel vm)
    {
        if (!ModelState.IsValid) return View(vm);
        if (await _db.Vehicles.AnyAsync(v => v.PlateNumber == vm.PlateNumber))
        {
            ModelState.AddModelError("PlateNumber", "이미 등록된 차량번호입니다.");
            return View(vm);
        }

        var v = new Vehicle
        {
            PlateNumber = vm.PlateNumber.Trim(),
            OwnerName = vm.OwnerName.Trim(),
            AccountNumber = vm.AccountNumber.Trim(),
            PhoneNumber = vm.PhoneNumber.Trim(),
            RegisteredAt = vm.RegisteredAt == default ? DateTime.UtcNow : vm.RegisteredAt,
            Memo = vm.Memo
        };
        _db.Vehicles.Add(v);
        await _db.SaveChangesAsync();
        TempData["Success"] = "차량이 등록되었습니다.";
        return RedirectToAction(nameof(Index));
    }

    [HttpGet]
    public async Task<IActionResult> Edit(string id)
    {
        if (string.IsNullOrEmpty(id)) return BadRequest();
        var veh = await _db.Vehicles.FirstOrDefaultAsync(v => v.PlateNumber == id);
        if (veh == null) return NotFound();

        var vm = new VehicleViewModel
        {
            Id = veh.Id,
            PlateNumber = veh.PlateNumber,
            OwnerName = veh.OwnerName,
            AccountNumber = veh.AccountNumber,
            PhoneNumber = veh.PhoneNumber,
            RegisteredAt = veh.RegisteredAt,
            Memo = veh.Memo
        };
        return View(vm);
    }

    [HttpPost]
    [ValidateAntiForgeryToken]
    public async Task<IActionResult> Edit(VehicleViewModel vm)
    {
        if (!ModelState.IsValid) return View(vm);

        var veh = await _db.Vehicles.FirstOrDefaultAsync(v => v.PlateNumber == vm.PlateNumber);
        if (veh == null) return NotFound();

        veh.OwnerName = vm.OwnerName.Trim();
        veh.AccountNumber = vm.AccountNumber.Trim();
        veh.PhoneNumber = vm.PhoneNumber.Trim();
        veh.RegisteredAt = vm.RegisteredAt;
        veh.Memo = vm.Memo;

        await _db.SaveChangesAsync();
        TempData["Success"] = "차량 정보가 수정되었습니다.";
        return RedirectToAction(nameof(Index));
    }

    [HttpPost]
    [ValidateAntiForgeryToken]
    public async Task<IActionResult> Delete(string id)
    {
        var veh = await _db.Vehicles.FirstOrDefaultAsync(v => v.PlateNumber == id);
        if (veh == null) return NotFound();

        var hasHistory = await _db.EntryExitRecords.AnyAsync(e => e.PlateNumber == id)
                       || await _db.Fees.AnyAsync(f => f.PlateNumber == id);
        if (hasHistory)
        {
            TempData["Error"] = "입출입/요금 내역이 있는 차량은 삭제할 수 없습니다.";
            return RedirectToAction(nameof(Index));
        }

        _db.Vehicles.Remove(veh);
        await _db.SaveChangesAsync();
        TempData["Success"] = "차량이 삭제되었습니다.";
        return RedirectToAction(nameof(Index));
    }
}
