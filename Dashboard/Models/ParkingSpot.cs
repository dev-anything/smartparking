using System.ComponentModel.DataAnnotations;
using System.ComponentModel.DataAnnotations.Schema;

namespace ParkingDashboard.Models;

/// <summary>
/// 주차 면 (자리) 상태. 초음파 센서 + 카메라가 보내오는 정보를 종합.
/// </summary>
public class ParkingSpot
{
    public int Id { get; set; }

    /// <summary>자리 번호 (예: A-1, B-3)</summary>
    [Required, StringLength(20)]
    public string SpotNumber { get; set; } = string.Empty;

    /// <summary>구역 (A/B/C)</summary>
    [Required, StringLength(5)]
    public string Zone { get; set; } = string.Empty;

    /// <summary>자리 활성화 여부</summary>
    public bool IsEnabled { get; set; } = true;

    /// <summary>마지막 점유 상태 (Empty / Occupied / Unknown)</summary>
    public SpotStatus Status { get; set; } = SpotStatus.Unknown;

    /// <summary>마지막 측정 시각</summary>
    public DateTime LastUpdated { get; set; } = DateTime.UtcNow;

    /// <summary>마지막 초음파 거리(cm) — 임계값 이하면 점유로 판정</summary>
    public double? LastDistanceCm { get; set; }

    /// <summary>현재 점유 중인 차량 번호 (출차 시점 알 수 없을 때는 Unknown)</summary>
    [StringLength(20)]
    public string? OccupiedByPlateNumber { get; set; }

    /// <summary>비고</summary>
    [StringLength(100)]
    public string? Memo { get; set; }
}

public enum SpotStatus
{
    Unknown = 0,
    Empty = 1,
    Occupied = 2
}
