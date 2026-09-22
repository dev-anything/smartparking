// 사이트 공통 JS (Chart.js 등 라이브러리는 _Layout에서 CDN으로 로드)
(function () {
    window.addEventListener('DOMContentLoaded', function () {
        // 자동 알림 닫기 (5초)
        document.querySelectorAll('.alert.alert-dismissible').forEach(function (el) {
            setTimeout(function () {
                var alert = bootstrap.Alert.getOrCreateInstance(el);
                alert.close();
            }, 5000);
        });
    });
})();
