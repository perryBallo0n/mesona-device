#!/system/bin/sh
export PATH=/system/bin:/system/xbin:$PATH
MARKER="/data/system/.gapps_firstboot_v1"

if [ -f "$MARKER" ]; then
    exit 0
fi

log -t gapps_firstboot "Waiting for GSF gservices.db and PackageManager..."
i=0
while [ $i -lt 45 ]; do
    if [ -f /data/data/com.google.android.gsf/databases/gservices.db ] && [ -d /data/data/com.oldmarket.vending236 ]; then
        break
    fi
    i=$((i + 1))
    sleep 2
done

# Give BOOT_COMPLETED receivers (GooglePartnerSetup) time to finish before overriding
sleep 12

if [ -f /data/data/com.google.android.gsf/databases/gservices.db ]; then
    sqlite3 /data/data/com.google.android.gsf/databases/gservices.db "INSERT OR REPLACE INTO overrides (name, value) VALUES ('vending_tos_version', '1');"
    sqlite3 /data/data/com.google.android.gsf/databases/gservices.db "INSERT OR REPLACE INTO overrides (name, value) VALUES ('vending_tos_url', 'http://api.oldmarket.xyz:5000');"
    sqlite3 /data/data/com.google.android.gsf/databases/gservices.db "INSERT OR REPLACE INTO overrides (name, value) VALUES ('gms_disable:com.google.android.youtube', 'enabled');"
    sqlite3 /data/data/com.google.android.gsf/databases/gservices.db "INSERT OR REPLACE INTO overrides (name, value) VALUES ('gms_disable:com.google.android.apps.books', 'enabled');"
fi

pm enable com.google.android.youtube
pm enable com.google.android.apps.books

if [ -d /data/data/com.oldmarket.vending236 ] && [ ! -f /data/data/com.oldmarket.vending236/shared_prefs/vending_preferences.xml ]; then
    rm /data/local/tmp/.v_own 2>/dev/null
    ls -l /data/data | while read perms own grp rest; do
        case "$rest" in
            *com.oldmarket.vending236)
                echo "$own $grp" > /data/local/tmp/.v_own
                ;;
        esac
    done
    if [ -f /data/local/tmp/.v_own ]; then
        read OWN GRP < /data/local/tmp/.v_own
        rm /data/local/tmp/.v_own
        mkdir /data/data/com.oldmarket.vending236/shared_prefs 2>/dev/null
        cat << 'EOF' > /data/data/com.oldmarket.vending236/shared_prefs/vending_preferences.xml
<?xml version='1.0' encoding='utf-8' standalone='yes' ?>
<map>
<string name="vending_tos_version">1</string>
</map>
EOF
        chmod 771 /data/data/com.oldmarket.vending236/shared_prefs
        chmod 660 /data/data/com.oldmarket.vending236/shared_prefs/vending_preferences.xml
        chown "$OWN.$GRP" /data/data/com.oldmarket.vending236/shared_prefs
        chown "$OWN.$GRP" /data/data/com.oldmarket.vending236/shared_prefs/vending_preferences.xml
    fi
fi

for pid in $(ps | while read u p rest; do case "$rest" in *vending236*|*gapps*) echo "$p";; esac; done); do
    kill "$pid"
done

echo "1" > "$MARKER"
chmod 644 "$MARKER"
log -t gapps_firstboot "Applied OldMarket TOS and GApps visibility defaults"
