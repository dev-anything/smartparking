using System.ComponentModel.DataAnnotations;

namespace ParkingDashboard.Models;

/// <summary>
/// 차량 정보 (차량 소유자 정보)
/// </summary>
public class Vehicle
{
    public int Id { get; set; }

    /// <summary>차량번호 (PK 역할, 예: 12가 3456)</summary>
    [Required, StringLength(20)]
    public string PlateNumber { get; set; } = string.Empty;

    /// <summary>차량 소유자 이름</summary>
    [Required, StringLength(50)]
    public string OwnerName { get; set; } = string.Empty;

    /// <summary>계좌번호 (정산용)</summary>
    [Required, StringLength(50)]
    public string AccountNumber { get; set; } = string.Empty;

    /// <summary>전화번호</summary>
    [Required, StringLength(20)]
    public string PhoneNumber { get; set; } = string.Empty;

    /// <summary>차량 등록일자</summary>
    public DateTime RegisteredAt { get; set; } = DateTime.UtcNow;

    /// <summary>비고 메모</summary>
    [StringLength(200)]
    public string? Memo { get; set; }

    // Navigation properties
    public ICollection<EntryExitRecord> EntryExitRecords { get; set; } = new List<EntryExitRecord>();
    public ICollection<Fee> Fees { get; set; } = new List<Fee>();
}
