using System.Text;
using Microsoft.EntityFrameworkCore;
using ParkingDashboard.Data;
using ParkingDashboard.ViewModels;

namespace ParkingDashboard.Services;

/// <summary>
/// 규칙 기반 LLM 서비스 (데이터 조회 어시스턴트)
/// - 한국어 자연어 질문에서 키워드/의도를 분석하여 DB 조회 후 답변을 생성합니다.
/// - 추후 OpenAI/Gemini/Claude 등 외부 LLM으로 손쉽게 교체 가능 (Provider 옵션으로 분기).
/// </summary>
public class RuleBasedLlmService : ILLMService
{
    private readonly ApplicationDbContext _db;

    public RuleBasedLlmService(ApplicationDbContext db) => _db = db;

    public async Task<ReportViewModel> AskAsync(string question, CancellationToken ct = default)
    {
        var vm = new ReportViewModel { Question = question, AskedAt = DateTime.Now };
        if (string.IsNullOrWhiteSpace(question))
        {
            vm.Answer = "질문을 입력해 주세요. 예) 오늘 입차 수, 이번 달 수익, 현재 주차 차량 수";
            return vm;
        }

        var q = question.ToLowerInvariant();
        var now = DateTime.Now;
        var todayStart = new DateTime(now.Year, now.Month, now.Day);
        var monthStart = new DateTime(now.Year, now.Month, 1);
        var weekStart = todayStart.AddDays(-(int)todayStart.DayOfWeek);

        try
        {
            // 1) 현재 주차 차량 수
            if (Contains(q, "주차", "현재", "남아") || Contains(q, "몇 대"))
            {
                var parkedCount = await _db.EntryExitRecords.CountAsync(e => e.ExitTime == null, ct);
                vm.Steps.Add(new ReportStep { Key = "current_parked", Label = "현재 주차 차량 수", Value = $"{parkedCount}대" });
                vm.Steps.Add(new ReportStep { Key = "available", Label = "빈 자리(추정)", Value = $"{(50 - parkedCount).ToString()}대" });
                vm.Answer = $"현재 주차 중인 차량은 **{parkedCount}대** 입니다. (전체 50자리 기준, 빈 자리 약 {50 - parkedCount}대)\n\n자세한 목록은 대시보드에서 확인 가능합니다.";
            }
            // 2) 오늘 입차 수
            else if (Contains(q, "오늘", "입차"))
            {
                var cnt = await _db.EntryExitRecords.CountAsync(e => e.EntryTime >= todayStart, ct);
                var sum = (await _db.Fees.Where(f => f.PaidAt >= todayStart).Select(f => f.Amount).ToListAsync(ct)).Sum();
                vm.Steps.Add(new ReportStep { Key = "today_entries", Label = "오늘 입차 수", Value = $"{cnt}건" });
                vm.Steps.Add(new ReportStep { Key = "today_revenue", Label = "오늘 수익", Value = $"{sum:N0}원" });
                vm.Answer = $"오늘({now:yyyy-MM-dd}) 기준 입차 건수는 **{cnt}건**, 누적 수익은 **{sum:N0}원** 입니다.";
            }
            // 3) 오늘 출차 / 오늘 수익
            else if (Contains(q, "오늘", "수익") || Contains(q, "오늘", "매출"))
            {
                var sum = (await _db.Fees.Where(f => f.PaidAt >= todayStart).Select(f => f.Amount).ToListAsync(ct)).Sum();
                var count = await _db.Fees.CountAsync(f => f.PaidAt >= todayStart, ct);
                vm.Steps.Add(new ReportStep { Key = "today_revenue", Label = "오늘 수익", Value = $"{sum:N0}원" });
                vm.Steps.Add(new ReportStep { Key = "today_paid_count", Label = "결제 건수", Value = $"{count}건" });
                vm.Answer = $"오늘 주차장 수익은 총 **{sum:N0}원** 입니다. (결제 건수: {count}건)";
            }
            // 4) 오늘 출차 수
            else if (Contains(q, "오늘", "출차"))
            {
                var cnt = await _db.EntryExitRecords.CountAsync(e => e.ExitTime != null && e.ExitTime >= todayStart, ct);
                vm.Steps.Add(new ReportStep { Key = "today_exits", Label = "오늘 출차 수", Value = $"{cnt}건" });
                vm.Answer = $"오늘 출차 처리된 차량은 **{cnt}건** 입니다.";
            }
            // 5) 이번 달 입차 / 수익
            else if ((Contains(q, "이번 달", "입차") || Contains(q, "월", "입차")))
            {
                var cnt = await _db.EntryExitRecords.CountAsync(e => e.EntryTime >= monthStart, ct);
                vm.Steps.Add(new ReportStep { Key = "month_entries", Label = "이번 달 입차", Value = $"{cnt}건" });
                vm.Answer = $"이번 달({now:yyyy-MM}) 누적 입차 건수는 **{cnt}건** 입니다.";
            }
            else if ((Contains(q, "이번 달", "수익") || Contains(q, "월", "수익") || Contains(q, "월", "매출")))
            {
                var sum = (await _db.Fees.Where(f => f.PaidAt >= monthStart).Select(f => f.Amount).ToListAsync(ct)).Sum();
                var cnt = await _db.Fees.CountAsync(f => f.PaidAt >= monthStart, ct);
                vm.Steps.Add(new ReportStep { Key = "month_revenue", Label = "이번 달 수익", Value = $"{sum:N0}원" });
                vm.Steps.Add(new ReportStep { Key = "month_count", Label = "이번 달 결제 건수", Value = $"{cnt}건" });
                vm.Answer = $"이번 달 주차장 수익은 **{sum:N0}원** 이며, 결제 건수는 **{cnt}건** 입니다.";
            }
            // 6) 이번 주 수익
            else if (Contains(q, "이번 주", "수익") || Contains(q, "주", "수익"))
            {
                var sum = (await _db.Fees.Where(f => f.PaidAt >= weekStart).Select(f => f.Amount).ToListAsync(ct)).Sum();
                vm.Steps.Add(new ReportStep { Key = "week_revenue", Label = "이번 주 수익", Value = $"{sum:N0}원" });
                vm.Answer = $"이번 주차장 수익은 **{sum:N0}원** 입니다.";
            }
            // 7) 등록 차량 수
            else if (Contains(q, "등록", "차량") || Contains(q, "회원"))
            {
                var cnt = await _db.Vehicles.CountAsync(ct);
                vm.Steps.Add(new ReportStep { Key = "registered", Label = "등록 차량 수", Value = $"{cnt}대" });
                vm.Answer = $"현재 등록된 차량은 총 **{cnt}대** 입니다.";
            }
            // 8) 특정 차량 검색 (예: "12가 3456 차량")
            else
            {
                var plateHit = ExtractPlateNumber(question);
                if (!string.IsNullOrEmpty(plateHit))
                {
                    var veh = await _db.Vehicles.FirstOrDefaultAsync(v => v.PlateNumber == plateHit, ct);
                    if (veh == null)
                    {
                        // 부분 검색
                        var any = await _db.Vehicles.Where(v => v.PlateNumber.Contains(plateHit)).ToListAsync(ct);
                        if (any.Any())
                        {
                            vm.Steps.Add(new ReportStep { Key = "vehicles", Label = "검색된 차량", Value = any.Select(v => $"{v.PlateNumber} - {v.OwnerName}").ToList() });
                            var sb = new StringBuilder();
                            sb.AppendLine($"'{plateHit}' 검색 결과: {any.Count}대");
                            foreach (var v in any.Take(10))
                                sb.AppendLine($"- {v.PlateNumber} / {v.OwnerName} / {v.PhoneNumber}");
                            vm.Answer = sb.ToString();
                        }
                        else
                        {
                            vm.Answer = $"'{plateHit}' 차량을 찾지 못했습니다. 차량 등록 여부를 확인해 주세요.";
                        }
                    }
                    else
                    {
                        var recCnt = await _db.EntryExitRecords.CountAsync(e => e.PlateNumber == plateHit, ct);
                        var feeSum = (await _db.Fees.Where(f => f.PlateNumber == plateHit).Select(f => f.Amount).ToListAsync(ct)).Sum();
                        vm.Steps.Add(new ReportStep { Key = "vehicle", Label = "차량", Value = $"{veh.PlateNumber} / {veh.OwnerName}" });
                        vm.Steps.Add(new ReportStep { Key = "history", Label = "입출입 건수", Value = $"{recCnt}건" });
                        vm.Steps.Add(new ReportStep { Key = "total_fee", Label = "누적 요금", Value = $"{feeSum:N0}원" });
                        vm.Answer = $"[{veh.PlateNumber}] {veh.OwnerName}님의 누적 입출입 {recCnt}건, 누적 요금 **{feeSum:N0}원** 입니다.";
                    }
                }
                else
                {
                    // 모호한 경우 안내
                    vm.Answer = "지원하는 질문 예시:\n" +
                                "- 오늘 입차 수는?\n" +
                                "- 오늘 수익 알려줘\n" +
                                "- 이번 달 수익은?\n" +
                                "- 현재 주차 차량 수\n" +
                                "- 등록 차량 수\n" +
                                "- [차량번호] 차량 정보 조회\n" +
                                "\n다시 질문해 주세요.";
                }
            }
        }
        catch (Exception ex)
        {
            vm.Answer = "데이터 조회 중 오류가 발생했습니다: " + ex.Message;
        }

        return vm;
    }

    private static bool Contains(string text, params string[] keywords)
        => keywords.All(k => text.Contains(k.ToLowerInvariant()));

    private static string? ExtractPlateNumber(string text)
    {
        // 한국 차량번호 패턴: 12가3456, 12가 3456, 123가4567 등
        var m = System.Text.RegularExpressions.Regex.Match(text, @"\d{2,3}\s?[가-힣]\s?\d{4}");
        if (m.Success) return m.Value.Replace(" ", "");
        // 단순히 4자리 이상 숫자가 있으면 그것도
        var m2 = System.Text.RegularExpressions.Regex.Match(text, @"\d{2,}[가-힣A-Za-z]\d{3,4}");
        return m2.Success ? m2.Value.Replace(" ", "") : null;
    }
}
