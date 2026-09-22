using System.ComponentModel.DataAnnotations;

namespace ParkingDashboard.ViewModels;

public class LoginViewModel
{
    [Required(ErrorMessage = "아이디를 입력하세요.")]
    public string Username { get; set; } = string.Empty;

    [Required(ErrorMessage = "비밀번호를 입력하세요.")]
    [DataType(DataType.Password)]
    public string Password { get; set; } = string.Empty;
}
