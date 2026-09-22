using ParkingDashboard.ViewModels;

namespace ParkingDashboard.Services;

public interface ILLMService
{
    /// <summary>자연어 질문을 받아 데이터 조회 결과를 자연스러운 답변으로 반환</summary>
    Task<ReportViewModel> AskAsync(string question, CancellationToken ct = default);
}
