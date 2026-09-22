using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;

namespace ParkingDashboard.Controllers;

/// <summary>
/// 시작 페이지 (루트 /) - 미로그인 시 로그인 페이지로 안내
/// </summary>
public class HomeController : Controller
{
    [AllowAnonymous]
    public IActionResult Index()
    {
        if (User.Identity?.IsAuthenticated == true)
            return RedirectToAction("Index", "Dashboard");
        return RedirectToAction("Login", "Account");
    }

    [AllowAnonymous]
    public IActionResult Privacy() => View();
}
