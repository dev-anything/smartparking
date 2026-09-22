using Microsoft.EntityFrameworkCore;
using ParkingDashboard.Models;

namespace ParkingDashboard.Data;

/// <summary>
/// EF Core DbContext - 주차장 관리 시스템의 모든 엔티티를 관리
/// </summary>
public class ApplicationDbContext : DbContext
{
    public ApplicationDbContext(DbContextOptions<ApplicationDbContext> options) : base(options) { }

    public DbSet<Vehicle> Vehicles => Set<Vehicle>();
    public DbSet<EntryExitRecord> EntryExitRecords => Set<EntryExitRecord>();
    public DbSet<Admin> Admins => Set<Admin>();
    public DbSet<Fee> Fees => Set<Fee>();
    public DbSet<ParkingSpot> ParkingSpots => Set<ParkingSpot>();
    public DbSet<BarrierGate> BarrierGates => Set<BarrierGate>();

    protected override void OnModelCreating(ModelBuilder modelBuilder)
    {
        base.OnModelCreating(modelBuilder);

        // Vehicle: PlateNumber 는 유니크
        modelBuilder.Entity<Vehicle>()
            .HasIndex(v => v.PlateNumber)
            .IsUnique();

        // Admin: Username 유니크
        modelBuilder.Entity<Admin>()
            .HasIndex(a => a.Username)
            .IsUnique();

        // EntryExitRecord: PlateNumber 인덱스 (조회 성능)
        modelBuilder.Entity<EntryExitRecord>()
            .HasIndex(e => e.PlateNumber);

        modelBuilder.Entity<EntryExitRecord>()
            .HasIndex(e => e.EntryTime);

        // Fee: PaidAt 인덱스
        modelBuilder.Entity<Fee>()
            .HasIndex(f => f.PaidAt);

        modelBuilder.Entity<Fee>()
            .HasIndex(f => f.PlateNumber);

        // 관계 설정
        modelBuilder.Entity<EntryExitRecord>()
            .HasOne(e => e.Vehicle)
            .WithMany(v => v.EntryExitRecords)
            .HasForeignKey(e => e.PlateNumber)
            .HasPrincipalKey(v => v.PlateNumber)
            .OnDelete(DeleteBehavior.Restrict);

        modelBuilder.Entity<Fee>()
            .HasOne(f => f.Vehicle)
            .WithMany(v => v.Fees)
            .HasForeignKey(f => f.PlateNumber)
            .HasPrincipalKey(v => v.PlateNumber)
            .OnDelete(DeleteBehavior.Restrict);

        modelBuilder.Entity<Fee>()
            .HasOne(f => f.EntryExitRecord)
            .WithMany()
            .HasForeignKey(f => f.EntryExitRecordId)
            .OnDelete(DeleteBehavior.SetNull);

        // ParkingSpot: SpotNumber 유니크
        modelBuilder.Entity<ParkingSpot>()
            .HasIndex(s => s.SpotNumber)
            .IsUnique();

        // BarrierGate: GateCode 유니크
        modelBuilder.Entity<BarrierGate>()
            .HasIndex(g => g.GateCode)
            .IsUnique();
    }
}
