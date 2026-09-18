#include "design_tokens.h"
#include "infra/config_manager.h"

DesignTokens DesignTokens::custom() {
    DesignTokens t;
    ConfigManager& cfg = ConfigManager::instance();
    
    // Load all colors from ConfigManager
    t.windowBg        = cfg.customTheme_windowBg();
    t.sidebarBg       = cfg.customTheme_sidebarBg();
    t.cardBg          = cfg.customTheme_cardBg();
    t.cardHover       = cfg.customTheme_cardHover();
    t.inputBg         = cfg.customTheme_inputBg();
    t.dialogBg        = cfg.customTheme_dialogBg();
    
    t.border          = cfg.customTheme_border();
    t.borderFocus     = cfg.customTheme_borderFocus();
    
    t.textPrimary     = cfg.customTheme_textPrimary();
    t.textSecondary   = cfg.customTheme_textSecondary();
    t.textDisabled    = cfg.customTheme_textDisabled();
    t.textOnPrimary   = cfg.customTheme_textOnPrimary();
    
    t.primary         = cfg.customTheme_primary();
    t.primaryHover    = cfg.customTheme_primaryHover();
    t.primaryPressed  = cfg.customTheme_primaryPressed();
    
    t.success         = cfg.customTheme_success();
    t.successBg       = cfg.customTheme_successBg();
    t.warning         = cfg.customTheme_warning();
    t.warningBg       = cfg.customTheme_warningBg();
    t.danger          = cfg.customTheme_danger();
    t.dangerBg        = cfg.customTheme_dangerBg();
    
    t.tableHeaderBg   = cfg.customTheme_tableHeaderBg();
    t.tableHeaderText = cfg.customTheme_tableHeaderText();
    t.tableRowAlt     = cfg.customTheme_tableRowAlt();
    t.tableRowHover   = cfg.customTheme_tableRowHover();
    t.tableSelected   = cfg.customTheme_tableSelected();
    t.tableBorder     = cfg.customTheme_tableBorder();
    
    t.navBg           = cfg.customTheme_navBg();
    t.navHover        = cfg.customTheme_navHover();
    t.navSelected     = cfg.customTheme_navSelected();
    t.navSelectedText = cfg.customTheme_navSelectedText();
    t.navText         = cfg.customTheme_navText();
    
    t.scrollHandle    = cfg.customTheme_scrollHandle();
    t.scrollTrack     = cfg.customTheme_scrollTrack();
    
    // Chart colors - use primary for consistency
    t.chartGrid       = t.border;
    t.chartAxisText   = t.textSecondary;
    t.chartLine       = t.primary;
    
    // Shadow - derive from primary color or use neutral
    t.shadowColor     = "rgba(0,0,0,0.10)";
    t.shadowBlur      = 20;
    t.shadowOffset    = 2;
    
    // Badges - use semantic colors
    t.badgePendingBg   = t.warningBg;   t.badgePendingText   = t.warning;
    t.badgeDeliveredBg = t.successBg;   t.badgeDeliveredText = t.success;
    t.badgeCancelledBg = t.dangerBg;    t.badgeCancelledText = t.danger;
    t.badgeOutBg       = t.tableSelected; t.badgeOutText     = t.primary;
    
    // Keep default spacing/sizing values
    // (these are typically not customized)
    
    return t;
}
