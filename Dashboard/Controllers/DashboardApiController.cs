using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;
using ParkingDashboard.Data;
using ParkingDashboard.Models;
using ParkingDashboard.Services;

namespace ParkingDashboard.Controllers;

/// <summary>
/// 대시보드용 API (외부 시스템에서 호출하는 REST 엔드포인트)
/// - GET  /api/parked-status  → 주차장 현황 조회
/// - POST /api/llm-query      → LLM 질문 요청
///
/// 사용자 시스템에서 다음과 같이 호출 가능:
///   curl http://192.168.0.7:10001/api/parked-status
///   curl -X POST -H "Content-Type: application/json" \
///        -d '{"question":"오늘 입차 수는?"}' \
///        http://192.168.0.7:10001/api/llm-query
/// </summary>
[ApiController]
[Route("api")]
public class DashboardApiController : ControllerBase
{
    private readonly ApplicationDbContext _db;
    private readonly ILLMService _llm;
    private readonly ILogger<DashboardApiController> _logger;

    public DashboardApiController(
        ApplicationDbContext db,
        ILLMService llm,
        ILogger<DashboardApiController> logger)
    {
        _db = db;
        _llm = llm;
        _logger = logger;
    }

    /// <summary>
    /// 주차장 현황 조회
    /// GET /api/parked-status
    /// </summary>
    [HttpGet("parked-status")]
    public async Task<IActionResult> ParkedStatus()
    {
        var now = DateTime.Now;
        var todayStart = new DateTime(now.Year, now.Month, now.Day);
        var monthStart = new DateTime(now.Year, now.Month, 1);

        // 자리 상태
        var spots = await _db.ParkingSpots
            .OrderBy(s => s.Zone).ThenBy(s => s.SpotNumber)
            .Select(s => new
            {
                spot_number = s.SpotNumber,
                zone = s.Zone,
                status = s.Status.ToString(),
                is_occupied = s.Status == SpotStatus.Occupied,
                distance_cm = s.LastDistanceCm,
                occupied_by = s.OccupiedByPlateNumber,
                last_updated = s.LastUpdated
            })
            .ToListAsync();

        // 현재 주차중 차량 (입차는 했지만 출차는 안 한)
        var currentVehicles = await _db.EntryExitRecords
            .Where(e => e.ExitTime == null)
            .OrderByDescending(e => e.EntryTime)
            .Select(e => new
            {
                plate_number = e.PlateNumber,
                entry_time = e.EntryTime,
                source = e.Source
            })
            .ToListAsync();

        // 통계 (SQLite decimal Sum 미지원 → 메모리 합산)
        int occupied = spots.Count(s => s.is_occupied);
        int empty = spots.Count(s => s.status == "Empty");
        int unknown = spots.Count(s => s.status == "Unknown");
        int total = spots.Count;

        int todayEntries = await _db.EntryExitRecords
            .CountAsync(e => e.EntryTime >= todayStart);
        int todayExits = await _db.EntryExitRecords
            .CountAsync(e => e.ExitTime != null && e.ExitTime >= todayStart);
        decimal todayRevenue = (await _db.Fees
            .Where(f => f.PaidAt >= todayStart)
            .Select(f => f.Amount)
            .ToListAsync()).Sum();
        decimal monthRevenue = (await _db.Fees
            .Where(f => f.PaidAt >= monthStart)
            .Select(f => f.Amount)
            .ToListAsync()).Sum();

        // 차단기
        var barriers = await _db.BarrierGates
            .OrderBy(g => g.GateCode)
            .Select(g => new
            {
                gate_code = g.GateCode,
                display_name = g.DisplayName,
                location = g.Location,
                status = g.Status.ToString(),
                last_changed = g.LastChanged,
                last_opened_at = g.LastOpenedAt,
                last_closed_at = g.LastClosedAt
            })
            .ToListAsync();

        return Ok(new
        {
            ok = true,
            timestamp = DateTime.UtcNow,
            server_time = now,
            total_spots = total,
            spots_summary = new
            {
                occupied = occupied,
                empty = empty,
                unknown = unknown,
                occupied_pct = total > 0
                    ? Math.Round((double)occupied / total * 100, 1)
                    : 0
            },
            spots = spots,
            current_vehicles = currentVehicles,
            stats = new
            {
                today_entries = todayEntries,
                today_exits = todayExits,
                today_revenue = todayRevenue,
                month_revenue = monthRevenue
            },
            barriers = barriers
        });
    }

    /// <summary>
    /// LLM 질문 요청
    /// POST /api/llm-query
    /// Body: { "question": "오늘 입차 수는?" }
    /// </summary>
    [HttpPost("llm-query")]
    public async Task<IActionResult> LlmQuery([FromBody] LlmQueryDto dto, CancellationToken ct)
    {
        if (dto == null || string.IsNullOrWhiteSpace(dto.Question))
            return BadRequest(new { ok = false, error = "question is required" });

        _logger.LogInformation("[LLM-QUERY] {Q}", dto.Question);

        try
        {
            var result = await _llm.AskAsync(dto.Question, ct);
            return Ok(new
            {
                ok = true,
                question = result.Question,
                answer = result.Answer,
                steps = result.Steps.Select(s => new
                {
                    key = s.Key,
                    label = s.Label,
                    value = s.Value?.ToString()
                }),
                asked_at = result.AskedAt
            });
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "LLM query 처리 중 오류");
            return StatusCode(500, new { ok = false, error = ex.Message });
        }
    }
}

/// <summary>LLM 질문 요청 Body DTO</summary>
public class LlmQueryDto
{
    public string Question { get; set; } = string.Empty;
}
