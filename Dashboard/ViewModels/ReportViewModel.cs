namespace ParkingDashboard.ViewModels;

public class ReportViewModel
{
    public string Question { get; set; } = string.Empty;
    public string Answer { get; set; } = string.Empty;
    public DateTime AskedAt { get; set; } = DateTime.UtcNow;
    public List<ReportStep> Steps { get; set; } = new();
}

public class ReportStep
{
    public string Key { get; set; } = string.Empty;   // e.g. "today_entries"
    public string Label { get; set; } = string.Empty;
    public object Value { get; set; } = string.Empty;
}
