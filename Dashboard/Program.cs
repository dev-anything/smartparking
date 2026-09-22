using Microsoft.AspNetCore.Authentication.Cookies;
using Microsoft.EntityFrameworkCore;
using ParkingDashboard.Data;
using ParkingDashboard.Services;

var builder = WebApplication.CreateBuilder(args);

// ----- Services -----
builder.Services.AddControllersWithViews();

// EF Core + SQLite
var conn = builder.Configuration.GetConnectionString("DefaultConnection")
            ?? "Data Source=parking.db";
builder.Services.AddDbContext<ApplicationDbContext>(opt => opt.UseSqlite(conn));

// 인증 (Cookie)
builder.Services.AddAuthentication(CookieAuthenticationDefaults.AuthenticationScheme)
    .AddCookie(options =>
    {
        options.LoginPath = "/Account/Login";
        options.AccessDeniedPath = "/Account/AccessDenied";
        options.ExpireTimeSpan = TimeSpan.FromHours(8);
        options.SlidingExpiration = true;
    });
builder.Services.AddAuthorization();

// LLM 서비스 선택 (RuleBased 가 기본, OpenAI 키가 있으면 OpenAI 사용)
var llmProvider = builder.Configuration.GetValue<string>("Llm:Provider") ?? "RuleBased";
var openAiKey = builder.Configuration["Llm:OpenApiKey"];
if (llmProvider == "OpenAI" && !string.IsNullOrWhiteSpace(openAiKey))
{
    builder.Services.AddSingleton<ILLMService, OpenAILlmService>();
}
else
{
    builder.Services.AddScoped<ILLMService, RuleBasedLlmService>();
}

var app = builder.Build();

// ----- Pipeline -----
if (!app.Environment.IsDevelopment())
{
    app.UseExceptionHandler("/Home/Error");
    app.UseHsts();
}

app.UseStaticFiles();
app.UseRouting();

app.UseAuthentication();
app.UseAuthorization();

app.MapControllerRoute(
    name: "default",
    pattern: "{controller=Home}/{action=Index}/{id?}");

// ----- DB 자동 생성 + 시드 -----
using (var scope = app.Services.CreateScope())
{
    var db = scope.ServiceProvider.GetRequiredService<ApplicationDbContext>();
    db.Database.EnsureCreated();
    await DbSeeder.SeedAsync(db);
}

app.Run();
