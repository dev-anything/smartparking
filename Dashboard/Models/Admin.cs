using System.ComponentModel.DataAnnotations;

namespace ParkingDashboard.Models;

/// <summary>
/// 관리자 계정
/// </summary>
public class Admin
{
    public int Id { get; set; }

    /// <summary>로그인 아이디</summary>
    [Required, StringLength(50)]
    public string Username { get; set; } = string.Empty;

    /// <summary>비밀번호 해시 (BCrypt)</summary>
    [Required, StringLength(255)]
    public string PasswordHash { get; set; } = string.Empty;

    /// <summary>관리자 이름</summary>
    [Required, StringLength(50)]
    public string FullName { get; set; } = string.Empty;

    /// <summary>관리자 연락처</summary>
    [Required, StringLength(20)]
    public string PhoneNumber { get; set; } = string.Empty;

    /// <summary>계정 생성일</summary>
    public DateTime CreatedAt { get; set; } = DateTime.UtcNow;

    /// <summary>마지막 로그인</summary>
    public DateTime? LastLoginAt { get; set; }

    /// <summary>활성 여부</summary>
    public bool IsActive { get; set; } = true;
}
