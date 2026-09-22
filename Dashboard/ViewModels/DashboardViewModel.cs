namespace ParkingDashboard.ViewModels;

/// <summary>
/// 메인 대시보드에서 보여줄 집계 데이터
/// </summary>
public class DashboardViewModel
{
    public int TotalSpots { get; set; }
    public int OccupiedSpots { get; set; }
    public int AvailableSpots => TotalSpots - OccupiedSpots;

    public int TodayEntries { get; set; }
    public int TodayExits { get; set; }
    public decimal TodayRevenue { get; set; }

    public int MonthEntries { get; set; }
    public decimal MonthRevenue { get; set; }

    public int RegisteredVehicles { get; set; }

    // 차트용 데이터
    public List<string> HourLabels { get; set; } = new();
    public List<int> HourlyEntries { get; set; } = new();

    public List<string> DayLabels { get; set; } = new();
    public List<decimal> DailyRevenue { get; set; } = new();

    public List<EntryExitSummary> RecentActivity { get; set; } = new();

    public List<CurrentParking> CurrentParkedList { get; set; } = new();

    // === 추가: 주차 자리(ESP32 초음파) ===
    public int SpotsOccupied { get; set; }      // 자리 점유 수 (센서 기반)
    public int SpotsEmpty { get; set; }         // 빈 자리 수
    public int SpotsUnknown { get; set; }      // 미확인 자리 수
    public List<ZoneStatus> Zones { get; set; } = new();
    public List<SpotView> AllSpots { get; set; } = new();
    public DateTime? LastSensorUpdate { get; set; }

    // === 추가: 차단기 (ESP32 서보) ===
    public List<BarrierView> Barriers { get; set; } = new();
}

public class EntryExitSummary
{
    public int Id { get; set; }
    public string PlateNumber { get; set; } = string.Empty;
    public DateTime EntryTime { get; set; }
    public DateTime? ExitTime { get; set; }
    public string Status { get; set; } = string.Empty;
    public string Source { get; set; } = string.Empty;
}

public class CurrentParking
{
    public string PlateNumber { get; set; } = string.Empty;
    public string OwnerName { get; set; } = string.Empty;
    public DateTime EntryTime { get; set; }
    public int DurationMinutes { get; set; }
}

public class ZoneStatus
{
    public string Zone { get; set; } = string.Empty;
    public int Total { get; set; }
    public int Empty { get; set; }
    public int Occupied { get; set; }
    public int Unknown { get; set; }
}

public class SpotView
{
    public string SpotNumber { get; set; } = string.Empty;
    public string Zone { get; set; } = string.Empty;
    public string Status { get; set; } = "Unknown";   // Empty / Occupied / Unknown
    public string? OccupiedByPlateNumber { get; set; }
    public double? LastDistanceCm { get; set; }
    public DateTime LastUpdated { get; set; }
}

public class BarrierView
{
    public string GateCode { get; set; } = string.Empty;        // ENTRY / EXIT
    public string DisplayName { get; set; } = string.Empty;
    public string? Location { get; set; }
    public string Status { get; set; } = "Unknown";             // Closed/Opening/Open/Closing/Error
    public DateTime LastChanged { get; set; }
    public DateTime? LastOpenedAt { get; set; }
    public DateTime? LastClosedAt { get; set; }
}
