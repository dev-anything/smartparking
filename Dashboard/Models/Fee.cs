using System.ComponentModel.DataAnnotations;
using System.ComponentModel.DataAnnotations.Schema;

namespace ParkingDashboard.Models;

/// <summary>
/// 요금 정산 내역
/// </summary>
public class Fee
{
    public int Id { get; set; }

    /// <summary>차량번호</summary>
    [Required, StringLength(20)]
    public string PlateNumber { get; set; } = string.Empty;

    /// <summary>입출입 기록 ID (참조)</summary>
    public int? EntryExitRecordId { get; set; }

    /// <summary>주차 시작 시각</summary>
    public DateTime EntryTime { get; set; }

    /// <summary>출차 시각</summary>
    public DateTime ExitTime { get; set; }

    /// <summary>총 주차 시간 (분)</summary>
    public int ParkedMinutes { get; set; }

    /// <summary>적용 요금 (원)</summary>
    [Column(TypeName = "decimal(10,0)")]
    public decimal Amount { get; set; }

    /// <summary>결제 수단 (현금/계좌이체/카드)</summary>
    [StringLength(20)]
    public string PaymentMethod { get; set; } = "계좌이체";

    /// <summary>결제 시각</summary>
    public DateTime PaidAt { get; set; } = DateTime.UtcNow;

    /// <summary>비고</summary>
    [StringLength(200)]
    public string? Note { get; set; }

    // Navigation
    public Vehicle? Vehicle { get; set; }
    public EntryExitRecord? EntryExitRecord { get; set; }
}
