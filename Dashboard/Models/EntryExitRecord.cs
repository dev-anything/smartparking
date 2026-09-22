using System.ComponentModel.DataAnnotations;
using System.ComponentModel.DataAnnotations.Schema;

namespace ParkingDashboard.Models;

/// <summary>
/// 입출입 기록
/// </summary>
public class EntryExitRecord
{
    public int Id { get; set; }

    /// <summary>차량번호 (Vehicle 과 연결)</summary>
    [Required, StringLength(20)]
    public string PlateNumber { get; set; } = string.Empty;

    /// <summary>입차시각</summary>
    public DateTime EntryTime { get; set; }

    /// <summary>출차시각 (아직 출차 안 했으면 null)</summary>
    public DateTime? ExitTime { get; set; }

    /// <summary>등록 출처 (Manual = 관리자 입력, Esp32 = 단말기, Auto = 기타)</summary>
    [StringLength(20)]
    public string Source { get; set; } = "Manual";

    /// <summary>비고</summary>
    [StringLength(200)]
    public string? Note { get; set; }

    // Navigation property
    public Vehicle? Vehicle { get; set; }

    /// <summary>출차 여부</summary>
    [NotMapped]
    public bool IsParked => ExitTime == null;

    /// <summary>주차 시간(분)</summary>
    [NotMapped]
    public int? DurationMinutes
    {
        get
        {
            if (ExitTime == null) return null;
            return (int)Math.Round((ExitTime.Value - EntryTime).TotalMinutes);
        }
    }
}
