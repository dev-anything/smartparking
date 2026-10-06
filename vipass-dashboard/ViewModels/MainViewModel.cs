using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace vipass_dashboard.ViewModels;

public partial class MainViewModel : ViewModelBase
{
    // 페이지 ViewModel은 한 번만 만들어 재사용한다. 메뉴를 오가도 입력 중이던 내용이 유지된다.
    private readonly DashboardViewModel _dashboard = new();
    private readonly RegisterCarInfoViewModel _registerCarInfo = new();

    // 우측 콘텐츠 영역에 표시할 현재 페이지. 값이 바뀌면 화면이 자동으로 갱신된다.
    [ObservableProperty]
    public partial ViewModelBase CurrentPage { get; set; }

    public MainViewModel()
    {
        CurrentPage = _dashboard;
    }

    [RelayCommand]
    private void NavigateToDashboard() => CurrentPage = _dashboard;

    [RelayCommand]
    private void NavigateToRegisterCarInfo() => CurrentPage = _registerCarInfo;
}
