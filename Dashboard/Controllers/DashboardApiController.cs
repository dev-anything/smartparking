using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;
using ParkingDashboard.Data;
using ParkingDashboard.Models;
using ParkingDashboard.Services;

namespace ParkingDashboard.Controllers;

/// <summary>
/// 대시보드용 API (외부 시스템에서 호출하는 REST 엔드포인트)
/// - GET  /api/parked-status      → 주차장 현황 조회
/// - POST /api/llm-query          → LLM 질문 요청 (자연어 답변)
/// - POST /api/llm-generate-sql   → LLM 에게 SQL 쿼리 생성 요청
/// - GET  /api/entry-exit-records → 입출입 기록 조회
///
/// 사용자 시스템에서 다음과 같이 호출 가능:
///   curl http://192.168.0.7:10001/api/parked-status
///   curl -X POST -H "Content-Type: application/json" \
///        -d '{"question":"오늘 입차 수는?"}' \
///        http://192.168.0.7:10001/api/llm-query
///   curl -X POST -H "Content-Type: application/json" \
///        -d '{"question":"오늘 입차한 차량 수"}' \
///        http://192.168.0.7:10001/api/llm-generate-sql
///   curl http://192.168.0.7:10001/api/entry-exit-records
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
    /// LLM 호출 요청 API
    /// POST /api/llm-query
    /// Body: { "question": "오늘 입차 수는?" }
    /// </summary>
    [HttpPost("llm-query")]
    public async Task<IActionResult> LlmQuery([FromBody] LlmQueryDto dto, CancellationToken ct)
    {
        const string SCHEMA = @"
vehicles(id, plate_number, owner_name, phone_number, vehicle_type, registered_at)
entryexitrecords(id, plate_number, entry_time, exit_time, source, note)
fees(id, plate_number, entry_exit_record_id, entry_time, exit_time, parked_minutes, amount, payment_method, paid_at, note)
";
        const string FEWSHOT_EXAMPLES = @"
Q: 오늘 입차한 차량 수
A: SELECT COUNT(*) FROM entryexitrecords WHERE DATE(entry_time) = DATE('now')
";

        if (dto == null || typeof(string) != dto.GetType() || string.IsNullOrWhiteSpace(dto.Question))
        {
            return BadRequest(new { answer = "question 필드가 필요합니다." });
        }

        var prompt =
            $"스키마: {SCHEMA}\n\n" +
            $"위 스카마만 사용해서 SQLite SELECT 쿼리를 작성해.\n\n" +
            $"규칙:\n" +
            $"1. SELECT만 사용. INSERT, UPDATE, DELETE, DROP 등은 절대 사용 금지.\n" +
            $"2. 스키마에 없는 테이블이나 컬럼은 사용 금지.\n" +
            $"3. 세미콜론으로 끝낼 것.\n" +
            $"4. 답은 SQL 문장 그 자체만 출력. 다른 글자, 기호, 줄바꿈도 앞뒤에 절대 붙이지 마.\n" +
            $"5. 출력 텍스트에 포함된 마크다운 문법은 모두 제거해.\n\n" +
            $"6. COUNT, SUM 등 집계 함수와 일반 컬럼을 함께 SELECT하지 마.\n" +
            $"7. 집계 함수만 쓰거나, 일반 컬럼만 쓰는 쿼리를 작성해.\n" +
            $"8. 입차 키워드는 entry_time 필드를 사용해.\n" +
            $"9. 출차 키워드는 exit_time 필드를 사용해.\n" +
            $"예시 출력:\n{FEWSHOT_EXAMPLES}\n\n" +
            $"질문: {dto.Question}";

        try
        {
            var rawSql = await _llm.AskSqlAsync(prompt, ct);
            return Content(rawSql, "text/plain");
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "LLM 호출 실패");
            return StatusCode(500, $"LLM 호출에 실패했습니다: {ex.Message}");
        }
    }

    /// <summary>
    /// LLM 에게 SQL 쿼리 생성 요청
    /// POST /api/llm-generate-sql
    /// Body: { "question": "오늘 입차한 차량 수" }
    /// Response: { "ok": true, "sql": "SELECT COUNT(*) FROM ..." }
    /// </summary>
    [HttpPost("llm-generate-sql")]
    public async Task<IActionResult> LlmGenerateSql([FromBody] LlmQueryDto dto, CancellationToken ct)
    {
        if (dto == null || string.IsNullOrWhiteSpace(dto.Question))
            return BadRequest(new { ok = false, error = "question is required" });

        var schema = @"
스키마:
vehicles(id, plate_number, owner_name, phone_number, vehicle_type, registered_at)
entryexitrecords(id, plate_number, entry_time, exit_time, source, note)
fees(id, plate_number, entry_exit_record_id, entry_time, exit_time, parked_minutes, amount, payment_method, paid_at, note)
";

        var prompt =
            $"{schema}\n\n" +
            $"위 스키마만 사용해서 SQLite SELECT 쿼리를 작성해.\n\n" +
            $"규칙:\n" +
            $"1. SELECT 만 사용. INSERT, UPDATE, DELETE, DROP 등은 절대 사용 금지.\n" +
            $"2. 스키마에 없는 테이블이나 컬럼은 사용 금지.\n" +
            $"3. 세미콜론으로 끝낼 것.\n" +
            $"4. 답은 SQL 문장 그 자체만 출력. 다른 글자, 기호, 줄바꿈도 앞뒤에 절대 붙이지 마.\n" +
            $"5. 출력 텍스트에 포함된 마크다운 문법은 모두 제거해.\n\n" +
            $"6. COUNT, SUM 등 집계 함수와 일반 컬럼을 함께 SELECT 하지 마.\n" +
            $"7. 집계 함수만 쓰거나, 일반 컬럼만 쓰는 쿼리를 작성해.\n" +
            $"8. 입차 키워드는 entry_time 필드를 사용해.\n" +
            $"9. 출차 키워드는 exit_time 필드를 사용해.\n\n" +
            $"예시:\n" +
            $"Q: 오늘 입차한 차량 수\n" +
            $"A: SELECT COUNT(*) FROM entryexitrecords WHERE DATE(entry_time) = DATE('now')\n\n" +
            $"질문: {dto.Question}";

        try
        {
            var sql = await _llm.AskSqlAsync(prompt, ct);
            return Ok(new { ok = true, sql = sql });
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "LLM SQL 생성 실패");
            return StatusCode(500, new { ok = false, error = ex.Message });
        }
    }

    /// <summary>
    /// LLM 자연어 질문 요청 (인증 없이 호출 가능)
    /// POST /api/llm-ask
    /// Body: { "question": "오늘 입차 수는?" }
    /// </summary>
    [AllowAnonymous]
    [HttpPost("llm-ask")]
    public async Task<IActionResult> LlmAsk([FromBody] LlmQueryDto dto, CancellationToken ct)
    {
        if (dto == null || string.IsNullOrWhiteSpace(dto.Question))
            return BadRequest(new { ok = false, error = "question is required" });

        _logger.LogInformation("[LLM-ASK] {Q} from {IP}", dto.Question, HttpContext.Connection.RemoteIpAddress);

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
            _logger.LogError(ex, "LLM ask 처리 중 오류");
            return StatusCode(500, new { ok = false, error = ex.Message });
        }
    }

    /// <summary>
    /// 입출입 기록 조회
    /// GET /api/entry-exit-records
    /// 쿼리 파라미터: ?page=1&limit=50&status=all|parked|completed&plateNumber=12가3456&from=2026-01-01&to=2026-12-31
    /// </summary>
    [HttpGet("entry-exit-records")]
    public async Task<IActionResult> EntryExitRecords(
        [FromQuery] int page = 1,
        [FromQuery] int limit = 50,
        [FromQuery] string status = "all",
        [FromQuery] string? plateNumber = null,
        [FromQuery] string? from = null,
        [FromQuery] string? to = null)
    {
        var now = DateTime.Now;
        var todayStart = new DateTime(now.Year, now.Month, now.Day);
        var monthStart = new DateTime(now.Year, now.Month, 1);
        var weekStart = todayStart.AddDays(-(int)todayStart.DayOfWeek);

        // 필터링 쿼리 구성
        var query = _db.EntryExitRecords.AsQueryable();

        // 상태 필터
        if (status == "parked")
            query = query.Where(e => e.ExitTime == null);
        else if (status == "completed")
            query = query.Where(e => e.ExitTime != null);

        // 차량번호 필터
        if (!string.IsNullOrWhiteSpace(plateNumber))
            query = query.Where(e => e.PlateNumber.Contains(plateNumber));

        // 날짜 범위 필터
        if (!string.IsNullOrWhiteSpace(from) && DateTime.TryParse(from, out var fromDate))
            query = query.Where(e => e.EntryTime >= fromDate);
        
        if (!string.IsNullOrWhiteSpace(to) && DateTime.TryParse(to, out var toDate))
            query = query.Where(e => e.EntryTime <= toDate);

        // 전체 개수
        var totalCount = await query.CountAsync();

        // 페이지네이션
        var records = await query
            .OrderByDescending(e => e.EntryTime)
            .Skip((page - 1) * limit)
            .Take(limit)
            .Select(e => new
            {
                id = e.Id,
                plate_number = e.PlateNumber,
                entry_time = e.EntryTime,
                exit_time = e.ExitTime,
                status = e.ExitTime == null ? "parked" : "completed",
                source = e.Source,
                note = e.Note,
                duration_minutes = e.ExitTime.HasValue 
                    ? (int)Math.Round((e.ExitTime.Value - e.EntryTime).TotalMinutes)
                    : (int?)null,
                owner_name = e.Vehicle != null ? e.Vehicle.OwnerName : null
            })
            .ToListAsync();

        // 통계
        var totalParked = await _db.EntryExitRecords.CountAsync(e => e.ExitTime == null);
        var totalCompleted = await _db.EntryExitRecords.CountAsync(e => e.ExitTime != null);
        var todayEntries = await _db.EntryExitRecords.CountAsync(e => e.EntryTime >= todayStart);
        var todayExits = await _db.EntryExitRecords.CountAsync(e => e.ExitTime != null && e.ExitTime >= todayStart);

        return Ok(new
        {
            ok = true,
            timestamp = DateTime.UtcNow,
            pagination = new
            {
                page = page,
                limit = limit,
                total = totalCount,
                pages = (int)Math.Ceiling((double)totalCount / limit)
            },
            stats = new
            {
                total_parked = totalParked,
                total_completed = totalCompleted,
                today_entries = todayEntries,
                today_exits = todayExits
            },
            records = records
        });
    }
}

/// <summary>LLM 질문 요청 Body DTO</summary>
public class LlmQueryDto
{
    public string Question { get; set; } = string.Empty;
}
