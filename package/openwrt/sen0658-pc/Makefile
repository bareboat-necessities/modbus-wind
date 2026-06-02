include $(TOPDIR)/rules.mk

PKG_NAME:=sen0658-pc
PKG_VERSION?=1.0.0
PKG_RELEASE?=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL?=file://$(TOPDIR)/dl
PKG_HASH?=skip
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/sen0658-pc
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=DFRobot SEN0658 Modbus RTU reader
  URL:=https://github.com/$(GITHUB_REPOSITORY)
  DEPENDS:=+libstdcpp
endef

define Package/sen0658-pc/description
  Command-line Modbus RTU polling tool for the DFRobot SEN0658 RS485
  weather sensor. It can print human-readable samples, emit NMEA 0183
  sentences, or serve NMEA data over TCP on OpenWrt routers.
endef

define Package/sen0658-pc/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/sen0658_poll $(1)/usr/bin/sen0658_poll
endef

$(eval $(call BuildPackage,sen0658-pc))
