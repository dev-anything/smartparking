using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;
using ParkingDashboard.Data;
using ParkingDashboard.ViewModels;

namespace ParkingDashboard.Controllers;

/// <summary>
/// 관리자 대시보드 메인 페이지
/// - 주차 현황 시각화 (시간대별 입차, 일별 수익, 현재 주차 차량)
/// </summary>
[Authorize]
public class DashboardController : Controller
{
    private readonly ApplicationDbContext _db;
    private readonly IConfiguration _cfg;

    public DashboardController(ApplicationDbContext db, IConfiguration cfg)
    {
        _db = db;
        _cfg = cfg;
    }

    public async Task<IActionResult> Index()
    {
        var totalSpots = _cfg.GetValue<int>("Parking:TotalSpots", 50);
        var now = DateTime.Now;
        var todayStart = new DateTime(now.Year, now.Month, now.Day);
        var monthStart = new DateTime(now.Year, now.Month, 1);

        // 현재 주차중 (출차 안 한 Entry)
        var parkedRecords = await _db.EntryExitRecords
            .Where(e => e.ExitTime == null)
            .OrderByDescending(e => e.EntryTime)
            .Take(50)
            .ToListAsync();

        // 차주의 Vehicle 매핑 (현재 주차 차량 표시에 사용)
        var plateList = parkedRecords.Select(p => p.PlateNumber).Distinct().ToList();
        var vehicles = await _db.Vehicles
            .Where(v => plateList.Contains(v.PlateNumber))
            .ToDictionaryAsync(v => v.PlateNumber, v => v);

        // 오늘 입출입 통계
        var todayEntries = await _db.EntryExitRecords
            .CountAsync(e => e.EntryTime >= todayStart);
        var todayExits = await _db.EntryExitRecords
            .CountAsync(e => e.ExitTime != null && e.ExitTime >= todayStart);

        // SQLite는 DB 레벨에서 decimal Sum을 직접 못 하므로 ToListAsync 후 메모리에서 합산
        var todayRevenue = (await _db.Fees
            .Where(f => f.PaidAt >= todayStart)
            .Select(f => f.Amount)
            .ToListAsync()).Sum();

        var monthEntries = await _db.EntryExitRecords
            .CountAsync(e => e.EntryTime >= monthStart);
        var monthRevenue = (await _db.Fees
            .Where(f => f.PaidAt >= monthStart)
            .Select(f => f.Amount)
            .ToListAsync()).Sum();

        var registeredVehicles = await _db.Vehicles.CountAsync();

        // 시간대별 입차 (오늘)
        var todayAll = await _db.EntryExitRecords
            .Where(e => e.EntryTime >= todayStart)
            .Select(e => e.EntryTime.Hour)
            .ToListAsync();
        var hourLabels = Enumerable.Range(0, 24).Select(h => $"{h:00}시").ToList();
        var hourlyEntries = Enumerable.Range(0, 24)
            .Select(h => todayAll.Count(x => x == h))
            .ToList();

        // 최근 14일 일별 수익 (SQLite는 decimal GroupBy Sum을 DB에서 못 하므로 메모리 처리)
        var dayStart = todayStart.AddDays(-13);
        var dailyFees = await _db.Fees
            .Where(f => f.PaidAt >= dayStart)
            .Select(f => new { f.PaidAt, f.Amount })
            .ToListAsync();
        var rawDaily = dailyFees
            .GroupBy(f => f.PaidAt.Date)
            .Select(g => new { Day = g.Key, Total = g.Sum(x => x.Amount) })
            .ToList();

        // 주차 자리 현황 (ESP32 초음파 센서 데이터)
        var allSpots = await _db.ParkingSpots
            .OrderBy(s => s.Zone)
            .ThenBy(s => s.SpotNumber)
            .ToListAsync();

        var spotViews = allSpots.Select(s => new SpotView
        {
            SpotNumber = s.SpotNumber,
            Zone = s.Zone,
            Status = s.Status.ToString(),
            OccupiedByPlateNumber = s.OccupiedByPlateNumber,
            LastDistanceCm = s.LastDistanceCm,
            LastUpdated = s.LastUpdated
        }).ToList();

        var spotsOccupied = allSpots.Count(s => s.Status == Models.SpotStatus.Occupied);
        var spotsEmpty = allSpots.Count(s => s.Status == Models.SpotStatus.Empty);
        var spotsUnknown = allSpots.Count(s => s.Status == Models.SpotStatus.Unknown);

        var zones = allSpots
            .GroupBy(s => s.Zone)
            .Select(g => new ZoneStatus
            {
                Zone = g.Key,
                Total = g.Count(),
                Occupied = g.Count(s => s.Status == Models.SpotStatus.Occupied),
                Empty = g.Count(s => s.Status == Models.SpotStatus.Empty),
                Unknown = g.Count(s => s.Status == Models.SpotStatus.Unknown)
            })
            .OrderBy(z => z.Zone)
            .ToList();

        var lastSensorUpdate = allSpots.Any()
            ? (DateTime?)allSpots.Max(s => s.LastUpdated)
            : null;

        // 차단기 상태
        var barrierEntities = await _db.BarrierGates
            .OrderBy(g => g.GateCode)
            .ToListAsync();
        var barriers = barrierEntities.Select(g => new BarrierView
        {
            GateCode = g.GateCode,
            DisplayName = g.DisplayName,
            Location = g.Location,
            Status = g.Status.ToString(),
            LastChanged = g.LastChanged,
            LastOpenedAt = g.LastOpenedAt,
            LastClosedAt = g.LastClosedAt
        }).ToList();
        var dayLabels = new List<string>();
        var dailyRevenue = new List<decimal>();
        for (int i = 0; i < 14; i++)
        {
            var d = dayStart.AddDays(i);
            dayLabels.Add(d.ToString("MM/dd"));
            dailyRevenue.Add(rawDaily.FirstOrDefault(x => x.Day == d)?.Total ?? 0m);
        }

        // 최근 활동 (최근 10건의 입출입)
        var recent = await _db.EntryExitRecords
            .OrderByDescending(e => e.EntryTime)
            .Take(10)
            .ToListAsync();

        var vm = new DashboardViewModel
        {
            TotalSpots = totalSpots,
            OccupiedSpots = parkedRecords.Count,
            TodayEntries = todayEntries,
            TodayExits = todayExits,
            TodayRevenue = todayRevenue,
            MonthEntries = monthEntries,
            MonthRevenue = monthRevenue,
            RegisteredVehicles = registeredVehicles,
            HourLabels = hourLabels,
            HourlyEntries = hourlyEntries,
            DayLabels = dayLabels,
            DailyRevenue = dailyRevenue,
            RecentActivity = recent.Select(e => new EntryExitSummary
            {
                Id = e.Id,
                PlateNumber = e.PlateNumber,
                EntryTime = e.EntryTime,
                ExitTime = e.ExitTime,
                Status = e.ExitTime == null ? "주차중" : "완료",
                Source = e.Source
            }).ToList(),
            CurrentParkedList = parkedRecords.Select(e => new CurrentParking
            {
                PlateNumber = e.PlateNumber,
                OwnerName = vehicles.TryGetValue(e.PlateNumber, out var v) ? v.OwnerName : "(미등록)",
                EntryTime = e.EntryTime,
                DurationMinutes = (int)Math.Round((now - e.EntryTime).TotalMinutes)
            }).ToList(),
            SpotsOccupied = spotsOccupied,
            SpotsEmpty = spotsEmpty,
            SpotsUnknown = spotsUnknown,
            Zones = zones,
            AllSpots = spotViews,
            LastSensorUpdate = lastSensorUpdate,
            Barriers = barriers
        };

        return View(vm);
    }
}
