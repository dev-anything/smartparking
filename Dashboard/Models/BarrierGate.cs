using System.ComponentModel.DataAnnotations;

namespace ParkingDashboard.Models;

/// <summary>
/// 입구 / 출구 차단기 상태 (서보 모터 기반)
/// </summary>
public class BarrierGate
{
    public int Id { get; set; }

    /// <summary>차단기 코드 (ENTRY=입구, EXIT=출구)</summary>
    [Required, StringLength(20)]
    public string GateCode { get; set; } = string.Empty;

    /// <summary>사용자에게 표시되는 이름</summary>
    [Required, StringLength(50)]
    public string DisplayName { get; set; } = string.Empty;

    /// <summary>현재 상태</summary>
    public GateStatus Status { get; set; } = GateStatus.Closed;

    /// <summary>마지막 변경 시각</summary>
    public DateTime LastChanged { get; set; } = DateTime.UtcNow;

    /// <summary>마지막 열림 시각</summary>
    public DateTime? LastOpenedAt { get; set; }

    /// <summary>마지막 닫힘 시각</summary>
    public DateTime? LastClosedAt { get; set; }

    /// <summary>활성 여부</summary>
    public bool IsEnabled { get; set; } = true;

    /// <summary>차단기 위치 (선택)</summary>
    [StringLength(50)]
    public string? Location { get; set; }

    /// <summary>비고</summary>
    [StringLength(200)]
    public string? Note { get; set; }
}

public enum GateStatus
{
    Unknown = 0,
    Closed = 1,
    Opening = 2,
    Open = 3,
    Closing = 4,
    Error = 5
}
