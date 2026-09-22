using System.ComponentModel.DataAnnotations;

namespace ParkingDashboard.ViewModels;

public class VehicleViewModel
{
    public int? Id { get; set; }

    [Required(ErrorMessage = "차량번호를 입력하세요.")]
    [Display(Name = "차량번호")]
    public string PlateNumber { get; set; } = string.Empty;

    [Required(ErrorMessage = "소유자 이름을 입력하세요.")]
    [Display(Name = "소유자 이름")]
    public string OwnerName { get; set; } = string.Empty;

    [Required(ErrorMessage = "계좌번호를 입력하세요.")]
    [Display(Name = "계좌번호")]
    public string AccountNumber { get; set; } = string.Empty;

    [Required(ErrorMessage = "전화번호를 입력하세요.")]
    [Display(Name = "전화번호")]
    public string PhoneNumber { get; set; } = string.Empty;

    [Display(Name = "등록일자")]
    public DateTime RegisteredAt { get; set; } = DateTime.UtcNow;

    [Display(Name = "비고")]
    public string? Memo { get; set; }
}
