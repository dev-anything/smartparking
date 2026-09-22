using Microsoft.EntityFrameworkCore;
using ParkingDashboard.Models;

namespace ParkingDashboard.Data;

/// <summary>
/// 초기 데이터 시드
/// - 첫 실행 시 관리자 계정/샘플 차량을 만들어 줍니다.
/// </summary>
public static class DbSeeder
{
    public static async Task SeedAsync(ApplicationDbContext db)
    {
        // 1) 관리자 계정이 없으면 기본 관리자 생성
        if (!await db.Admins.AnyAsync())
        {
            var admin = new Admin
            {
                Username = "admin",
                // 기본 비밀번호: admin1234 (BCrypt 해시)
                PasswordHash = BCrypt.Net.BCrypt.HashPassword("admin1234"),
                FullName = "시스템 관리자",
                PhoneNumber = "010-0000-0000",
                CreatedAt = DateTime.UtcNow,
                IsActive = true
            };
            db.Admins.Add(admin);
        }

        // 2) 샘플 차량 (테스트용) - 이미 등록된 차량이 없으면 생성
        if (!await db.Vehicles.AnyAsync())
        {
            db.Vehicles.AddRange(
                new Vehicle
                {
                    PlateNumber = "12가 3456",
                    OwnerName = "홍길동",
                    AccountNumber = "110-123-456789",
                    PhoneNumber = "010-1234-5678",
                    RegisteredAt = DateTime.UtcNow.AddDays(-30),
                    Memo = "정기 주차 회원"
                },
                new Vehicle
                {
                    PlateNumber = "34나 5678",
                    OwnerName = "김철수",
                    AccountNumber = "110-234-567890",
                    PhoneNumber = "010-2345-6789",
                    RegisteredAt = DateTime.UtcNow.AddDays(-10),
                    Memo = ""
                }
            );
        }

        // 3) 주차 자리 시드 (총 6개: L존 3개 + R존 3개 — 가운데 통로, 입구 아래)
        if (!await db.ParkingSpots.AnyAsync())
        {
            var spots = new List<ParkingSpot>();
            foreach (var zone in new[] { "L", "R" })
            {
                for (int i = 1; i <= 3; i++)
                {
                    spots.Add(new ParkingSpot
                    {
                        SpotNumber = $"{zone}-{i}",
                        Zone = zone,
                        Status = SpotStatus.Unknown,
                        LastUpdated = DateTime.UtcNow
                    });
                }
            }
            db.ParkingSpots.AddRange(spots);
        }

        // 4) 차단기 시드 (입구 / 출구)
        if (!await db.BarrierGates.AnyAsync())
        {
            db.BarrierGates.AddRange(
                new BarrierGate
                {
                    GateCode = "ENTRY",
                    DisplayName = "입구 차단기",
                    Location = "주차장 입구 (정문)",
                    Status = GateStatus.Closed,
                    LastChanged = DateTime.UtcNow,
                    LastClosedAt = DateTime.UtcNow
                },
                new BarrierGate
                {
                    GateCode = "EXIT",
                    DisplayName = "출구 차단기",
                    Location = "주차장 출구",
                    Status = GateStatus.Closed,
                    LastChanged = DateTime.UtcNow,
                    LastClosedAt = DateTime.UtcNow
                }
            );
        }

        await db.SaveChangesAsync();
    }
}
