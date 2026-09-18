#pragma once
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// Single source of truth for all visual tokens.
// ThemeManager reads these — nothing else should hardcode hex values.
// ─────────────────────────────────────────────────────────────────────────────

enum class ThemeType { Light, Dark, Custom };

struct DesignTokens {
    // ── Surfaces ──────────────────────────────────────────────────────────────
    QString windowBg;
    QString sidebarBg;
    QString cardBg;
    QString cardHover;
    QString inputBg;
    QString dialogBg;

    // ── Borders ───────────────────────────────────────────────────────────────
    QString border;
    QString borderFocus;

    // ── Text ──────────────────────────────────────────────────────────────────
    QString textPrimary;
    QString textSecondary;
    QString textDisabled;
    QString textOnPrimary;   // text on top of primary-colored buttons

    // ── Brand / Accent ────────────────────────────────────────────────────────
    QString primary;
    QString primaryHover;
    QString primaryPressed;

    // ── Semantic ──────────────────────────────────────────────────────────────
    QString success;
    QString successBg;
    QString warning;
    QString warningBg;
    QString danger;
    QString dangerBg;

    // ── Table ─────────────────────────────────────────────────────────────────
    QString tableHeaderBg;
    QString tableHeaderText;
    QString tableRowAlt;
    QString tableRowHover;
    QString tableSelected;
    QString tableBorder;

    // ── Sidebar ───────────────────────────────────────────────────────────────
    QString navBg;
    QString navHover;
    QString navSelected;
    QString navSelectedText;
    QString navText;

    // ── Charts ────────────────────────────────────────────────────────────────
    QString chartGrid;
    QString chartAxisText;
    QString chartLine;

    // ── Shadows ───────────────────────────────────────────────────────────────
    QString shadowColor;      // CSS rgba string e.g. "rgba(0,0,0,0.10)"
    int     shadowBlur = 20;
    int     shadowOffset = 2;

    // ── Scrollbar ─────────────────────────────────────────────────────────────
    QString scrollHandle;
    QString scrollTrack;

    // ── Status badges ─────────────────────────────────────────────────────────
    QString badgePendingBg;     QString badgePendingText;
    QString badgeDeliveredBg;   QString badgeDeliveredText;
    QString badgeCancelledBg;   QString badgeCancelledText;
    QString badgeOutBg;         QString badgeOutText;

    // ── Spacing (px) — 4/8-grid ───────────────────────────────────────────────
    int sp1  =  4;
    int sp2  =  8;
    int sp3  = 12;
    int sp4  = 16;
    int sp5  = 24;
    int sp6  = 32;

    // ── Radius ────────────────────────────────────────────────────────────────
    int radiusSm =  8;
    int radiusMd = 12;
    int radiusLg = 16;

    // ── Typography ────────────────────────────────────────────────────────────
    int fontH1   = 22;   // Bold
    int fontH2   = 18;   // SemiBold
    int fontBody = 14;   // Regular
    int fontSm   = 12;   // Regular (caption)

    // ── Input height ──────────────────────────────────────────────────────────
    int inputHeight = 40;

    // ── Table row / header heights ────────────────────────────────────────────
    int tableHeaderHeight = 42;
    int tableRowHeight    = 40;

    // ── Sidebar widths ────────────────────────────────────────────────────────
    int sidebarExpanded  = 210;
    int sidebarCollapsed =  56;

    // ── Factory methods ───────────────────────────────────────────────────────
    static DesignTokens light() {
        DesignTokens t;
        t.windowBg        = "#F8FAFC";
        t.sidebarBg       = "#FFFFFF";
        t.cardBg          = "#FFFFFF";
        t.cardHover       = "#F1F5F9";
        t.inputBg         = "#FFFFFF";
        t.dialogBg        = "#FFFFFF";

        t.border          = "#E5E7EB";
        t.borderFocus     = "#3B82F6";

        t.textPrimary     = "#0F172A";
        t.textSecondary   = "#64748B";
        t.textDisabled    = "#CBD5E1";
        t.textOnPrimary   = "#FFFFFF";

        t.primary         = "#3B82F6";
        t.primaryHover    = "#2563EB";
        t.primaryPressed  = "#1D4ED8";

        t.success         = "#22C55E";  t.successBg  = "#DCFCE7";
        t.warning         = "#F59E0B";  t.warningBg  = "#FEF3C7";
        t.danger          = "#EF4444";  t.dangerBg   = "#FEE2E2";

        t.tableHeaderBg   = "#F1F5F9";
        t.tableHeaderText = "#475569";
        t.tableRowAlt     = "#F8FAFC";
        t.tableRowHover   = "#EFF6FF";
        t.tableSelected   = "#DBEAFE";
        t.tableBorder     = "#E5E7EB";

        t.navBg           = "#FFFFFF";
        t.navHover        = "#F1F5F9";
        t.navSelected     = "#3B82F6";
        t.navSelectedText = "#FFFFFF";
        t.navText         = "#64748B";

        t.chartGrid       = "#E2E8F0";
        t.chartAxisText   = "#64748B";
        t.chartLine       = "#3B82F6";

        t.shadowColor     = "rgba(0,0,0,0.10)";
        t.shadowBlur      = 20;  t.shadowOffset = 2;

        t.scrollHandle    = "#CBD5E1";
        t.scrollTrack     = "#F1F5F9";

        t.badgePendingBg   = "#FEF3C7"; t.badgePendingText   = "#92400E";
        t.badgeDeliveredBg = "#DCFCE7"; t.badgeDeliveredText = "#166534";
        t.badgeCancelledBg = "#FEE2E2"; t.badgeCancelledText = "#991B1B";
        t.badgeOutBg       = "#DBEAFE"; t.badgeOutText       = "#1E40AF";
        return t;
    }

    static DesignTokens dark() {
        DesignTokens t;
        // ── Indigo command-center dark theme ──────────────────────────────────
        t.windowBg        = "#0D1122";
        t.sidebarBg       = "#151A33";
        t.cardBg          = "#151A33";
        t.cardHover       = "#1C2240";
        t.inputBg         = "#1C2240";
        t.dialogBg        = "#151A33";

        t.border          = "#232A47";
        t.borderFocus     = "#4C4FE0";

        t.textPrimary     = "#E8EAF6";
        t.textSecondary   = "#8A8FBF";
        t.textDisabled    = "#3A3F6B";
        t.textOnPrimary   = "#FFFFFF";

        t.primary         = "#4C4FE0";
        t.primaryHover    = "#6366F1";
        t.primaryPressed  = "#3730A3";

        t.success         = "#4ADE94";  t.successBg  = "#1E3A34";
        t.warning         = "#E0B84B";  t.warningBg  = "#3A331A";
        t.danger          = "#F0645C";  t.dangerBg   = "#3A1E22";

        t.tableHeaderBg   = "#151A33";
        t.tableHeaderText = "#8A8FBF";
        t.tableRowAlt     = "#111628";
        t.tableRowHover   = "#1C2240";
        t.tableSelected   = "#232A47";
        t.tableBorder     = "#232A47";

        t.navBg           = "#151A33";
        t.navHover        = "#1C2240";
        t.navSelected     = "#232A47";
        t.navSelectedText = "#8C8FF0";
        t.navText         = "#8A8FBF";

        t.chartGrid       = "#232A47";
        t.chartAxisText   = "#8A8FBF";
        t.chartLine       = "#4C4FE0";

        t.shadowColor     = "rgba(76,79,224,0.10)";
        t.shadowBlur      = 16;  t.shadowOffset = 1;

        t.scrollHandle    = "#232A47";
        t.scrollTrack     = "#151A33";

        t.badgePendingBg   = "#3A331A"; t.badgePendingText   = "#E0B84B";
        t.badgeDeliveredBg = "#1E3A34"; t.badgeDeliveredText = "#4ADE94";
        t.badgeCancelledBg = "#3A1E22"; t.badgeCancelledText = "#F0645C";
        t.badgeOutBg       = "#232A47"; t.badgeOutText       = "#8C8FF0";

        t.radiusSm =  8;
        t.radiusMd = 10;
        t.radiusLg = 14;
        return t;
    }

    // Custom theme (loaded from ConfigManager)
    static DesignTokens custom();

    static DesignTokens forTheme(ThemeType type) {
        switch (type) {
            case ThemeType::Light:  return light();
            case ThemeType::Dark:   return dark();
            case ThemeType::Custom: return custom();
        }
        return dark();
    }
};
