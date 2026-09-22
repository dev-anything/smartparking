using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;
using ParkingDashboard.Data;
using ParkingDashboard.Models;

namespace ParkingDashboard.Controllers;

/// <summary>
/// ESP32 / IoT 단말기 연동용 REST API.
/// - 외부 단말기에서 호출할 엔드포인트를 제공합니다.
/// - 추후 ESP32 IP 화이트리스트 또는 키 기반 인증을 추가하세요.
///
/// 활성화:  appsettings.json 의 Esp32.Enabled = true
/// 주소:    appsettings.json 의 Esp32.Endpoint = "http://ESP32_IP"
/// </summary>
[ApiController]
[Route("api/esp32")]
public class ApiController : ControllerBase
{
    private readonly ApplicationDbContext _db;
    private readonly IConfiguration _cfg;
    private readonly ILogger<ApiController> _logger;

    public ApiController(ApplicationDbContext db, IConfiguration cfg, ILogger<ApiController> logger)
    {
        _db = db;
        _cfg = cfg;
        _logger = logger;
    }

    // ============= [Ping] =============

    /// <summary>단말기 상태 확인</summary>
    [HttpGet("ping")]
    public IActionResult Ping() => Ok(new
    {
        ok = true,
        esp32Enabled = _cfg.GetValue<bool>("Esp32:Enabled"),
        endpoint = _cfg["Esp32:Endpoint"] ?? "",
        serverTime = DateTime.UtcNow
    });

    // ============= [초음파 센서] =============

    /// <summary>
    /// ESP32 초음파 센서 → 거리값 전송
    /// POST /api/esp32/spot/distance
    /// Body: { "spotNumber": "A-1", "distanceCm": 27.5, "plateNumber": null }
    /// threshold(cm) 이하이면 Occupied, 이상이면 Empty 로 판정
    /// </summary>
    [HttpPost("spot/distance")]
    public async Task<IActionResult> SpotDistance([FromBody] SpotDistanceDto dto)
    {
        if (dto == null || string.IsNullOrWhiteSpace(dto.SpotNumber))
            return BadRequest(new { ok = false, error = "spotNumber required" });

        var spot = await _db.ParkingSpots.FirstOrDefaultAsync(s => s.SpotNumber == dto.SpotNumber);
        if (spot == null)
            return Ok(new { ok = false, message = $"자리 {dto.SpotNumber} 미등록" });

        var threshold = _cfg.GetValue<double?>("Parking:UltrasonicThresholdCm") ?? 50.0;
        var prevStatus = spot.Status;

        spot.LastDistanceCm = dto.DistanceCm;
        spot.LastUpdated = DateTime.UtcNow;

        if (dto.DistanceCm <= threshold)
        {
            spot.Status = SpotStatus.Occupied;
            if (!string.IsNullOrWhiteSpace(dto.PlateNumber))
                spot.OccupiedByPlateNumber = dto.PlateNumber.Trim();  // DB와 동일한 형식
        }
        else
        {
            spot.Status = SpotStatus.Empty;
            // 비어있을 때는 점유 차량 정보도 해제
            spot.OccupiedByPlateNumber = null;
        }

        await _db.SaveChangesAsync();

        if (prevStatus != spot.Status)
            _logger.LogInformation("[초음파] 자리 {Spot} {Prev}→{Now} ({Dist}cm, 차량={Plate})",
                spot.SpotNumber, prevStatus, spot.Status, dto.DistanceCm, spot.OccupiedByPlateNumber ?? "-");

        return Ok(new
        {
            ok = true,
            spotNumber = spot.SpotNumber,
            status = spot.Status.ToString(),
            distanceCm = spot.LastDistanceCm,
            threshold,
            occupiedBy = spot.OccupiedByPlateNumber
        });
    }

    /// <summary>현재 모든 주차 자리 상태</summary>
    [HttpGet("spots")]
    public async Task<IActionResult> Spots()
    {
        var spots = await _db.ParkingSpots
            .OrderBy(s => s.Zone)
            .ThenBy(s => s.SpotNumber)
            .Select(s => new
            {
                s.SpotNumber, s.Zone, s.IsEnabled,
                Status = s.Status.ToString(),
                s.LastUpdated, s.LastDistanceCm, s.OccupiedByPlateNumber
            })
            .ToListAsync();
        return Ok(new { ok = true, count = spots.Count, data = spots });
    }

    // ============= [카메라(차단기)] =============

    /// <summary>
    /// ESP32 카메라(차단기) → 차량번호 인식 후 입차 처리
    /// POST /api/esp32/camera/entry
    /// Body: { "plateNumber": "12가 3456", "confidence": 0.95, "imageUrl": "https://...", "spotNumber": "A-1" }
    /// </summary>
    [HttpPost("camera/entry")]
    public async Task<IActionResult> CameraEntry([FromBody] CameraEntryDto dto)
    {
        if (dto == null || string.IsNullOrWhiteSpace(dto.PlateNumber))
            return BadRequest(new { ok = false, error = "plateNumber required" });

        // 중복 입차 체크 (이미 주차중)
        var already = await _db.EntryExitRecords
            .FirstOrDefaultAsync(e => e.PlateNumber == dto.PlateNumber && e.ExitTime == null);
        if (already != null)
            return Ok(new { ok = true, message = "이미 주차중", id = already.Id, duplicated = true });

        // PlateNumber 는 DB(Vehicle)에 저장된 그대로 사용 (공백 포함) → FK 매칭
        var plate = dto.PlateNumber.Trim();
        var entryTime = dto.Timestamp ?? DateTime.UtcNow;

        var record = new EntryExitRecord
        {
            PlateNumber = plate,
            EntryTime = entryTime,
            Source = "Esp32-Camera",
            Note = $"신뢰도={dto.Confidence:F2}{(string.IsNullOrEmpty(dto.ImageUrl) ? "" : " | " + dto.ImageUrl)}"
        };
        _db.EntryExitRecords.Add(record);

        // spotNumber 있으면 자리도 점유 처리
        if (!string.IsNullOrWhiteSpace(dto.SpotNumber))
        {
            var spot = await _db.ParkingSpots.FirstOrDefaultAsync(s => s.SpotNumber == dto.SpotNumber);
            if (spot != null)
            {
                spot.Status = SpotStatus.Occupied;
                spot.OccupiedByPlateNumber = plate;
                spot.LastUpdated = DateTime.UtcNow;
            }
        }

        await _db.SaveChangesAsync();

        _logger.LogInformation("[카메라] 입차: {Plate} (conf={Conf}) at {Spot}",
            plate, dto.Confidence, dto.SpotNumber ?? "-");

        return Ok(new
        {
            ok = true, id = record.Id, plateNumber = plate,
            entryTime = record.EntryTime, confidence = dto.Confidence
        });
    }

    /// <summary>
    /// ESP32 카메라(차단기) → 차량번호 인식 후 출차 처리
    /// POST /api/esp32/camera/exit
    /// Body: { "plateNumber": "12가 3456", "confidence": 0.95, "amount": 3000, "spotNumber": "A-1" }
    /// </summary>
    [HttpPost("camera/exit")]
    public async Task<IActionResult> CameraExit([FromBody] CameraExitDto dto)
    {
        if (dto == null || string.IsNullOrWhiteSpace(dto.PlateNumber))
            return BadRequest(new { ok = false, error = "plateNumber required" });

        // DB의 PlateNumber 형식 그대로 유지 (Trim만)
        var plate = dto.PlateNumber.Trim();
        var record = await _db.EntryExitRecords
            .FirstOrDefaultAsync(e => e.PlateNumber == plate && e.ExitTime == null);

        if (record == null)
            return Ok(new { ok = true, message = "해당 차량의 입차 기록 없음" });

        var now = dto.Timestamp ?? DateTime.UtcNow;
        record.ExitTime = now;

        var minutes = (int)Math.Round((now - record.EntryTime).TotalMinutes);
        decimal amount = dto.Amount ?? CalcFee(minutes);

        var fee = new Fee
        {
            PlateNumber = plate,
            EntryExitRecordId = record.Id,
            EntryTime = record.EntryTime,
            ExitTime = now,
            ParkedMinutes = minutes,
            Amount = amount,
            PaymentMethod = string.IsNullOrWhiteSpace(dto.PaymentMethod) ? "계좌이체" : dto.PaymentMethod,
            PaidAt = now,
            Note = $"신뢰도={dto.Confidence:F2}"
        };
        _db.Fees.Add(fee);

        // 자리 해제
        if (!string.IsNullOrWhiteSpace(dto.SpotNumber))
        {
            var spot = await _db.ParkingSpots.FirstOrDefaultAsync(s => s.SpotNumber == dto.SpotNumber);
            if (spot != null)
            {
                spot.Status = SpotStatus.Empty;
                spot.OccupiedByPlateNumber = null;
                spot.LastUpdated = DateTime.UtcNow;
            }
        }
        else
        {
            // 자리 미지정 시 같은 차량 점유 중인 자리 모두 해제
            var spots = await _db.ParkingSpots
                .Where(s => s.OccupiedByPlateNumber == plate)
                .ToListAsync();
            foreach (var s in spots)
            {
                s.Status = SpotStatus.Empty;
                s.OccupiedByPlateNumber = null;
                s.LastUpdated = DateTime.UtcNow;
            }
        }

        await _db.SaveChangesAsync();

        _logger.LogInformation("[카메라] 출차: {Plate} (conf={Conf}) {Amount}원",
            plate, dto.Confidence, amount);

        return Ok(new { ok = true, plateNumber = plate, minutes, amount });
    }

    // ============= [단순 문 매니페스트] =============

    /// <summary>차량 정보 조회</summary>
    [HttpGet("vehicle/{plateNumber}")]
    public async Task<IActionResult> Vehicle(string plateNumber)
    {
        var v = await _db.Vehicles.FirstOrDefaultAsync(x => x.PlateNumber == plateNumber);
        if (v == null) return NotFound(new { ok = false });
        return Ok(new { ok = true, data = v });
    }

    /// <summary>레거시 통합 entry (수동 차고등 단말기)</summary>
    [HttpPost("entry")]
    public async Task<IActionResult> Entry([FromBody] EntryDto dto)
        => await CameraEntry(new CameraEntryDto
        {
            PlateNumber = dto.PlateNumber,
            Source = dto.Source,
            Note = dto.Note
        });

    /// <summary>레거시 통합 exit</summary>
    [HttpPost("exit")]
    public async Task<IActionResult> Exit([FromBody] ExitDto dto)
        => await CameraExit(new CameraExitDto
        {
            PlateNumber = dto.PlateNumber,
            Amount = dto.Amount,
            PaymentMethod = dto.PaymentMethod
        });

    // ============= [내부 헬퍼 + DTO] =============

    private static decimal CalcFee(int minutes)
    {
        const int free = 15;
        const int hourly = 1000;
        if (minutes <= free) return 0m;
        return (decimal)Math.Ceiling(minutes / 60.0) * hourly;
    }

    public class EntryDto
    {
        public string PlateNumber { get; set; } = string.Empty;
        public string? Source { get; set; }
        public string? Note { get; set; }
    }

    public class ExitDto
    {
        public string PlateNumber { get; set; } = string.Empty;
        public decimal? Amount { get; set; }
        public string? PaymentMethod { get; set; }
    }

    public class SpotDistanceDto
    {
        public string SpotNumber { get; set; } = string.Empty;
        public double DistanceCm { get; set; }
        public string? PlateNumber { get; set; }
    }

    public class CameraEntryDto
    {
        public string PlateNumber { get; set; } = string.Empty;
        public double Confidence { get; set; } = 1.0;
        public string? ImageUrl { get; set; }
        public string? SpotNumber { get; set; }
        public DateTime? Timestamp { get; set; }
        public string? Source { get; set; }   // 내부 전환용
        public string? Note { get; set; }
    }

    public class CameraExitDto
    {
        public string PlateNumber { get; set; } = string.Empty;
        public double Confidence { get; set; } = 1.0;
        public decimal? Amount { get; set; }
        public string? PaymentMethod { get; set; }
        public string? SpotNumber { get; set; }
        public DateTime? Timestamp { get; set; }
    }
}
