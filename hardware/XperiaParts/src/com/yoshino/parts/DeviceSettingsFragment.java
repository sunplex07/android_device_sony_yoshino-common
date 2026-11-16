/*
 * Copyright (c) 2020, Shashank Verma (shank03) <shashank.verma2002@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 */

package com.yoshino.parts;

import android.app.AlertDialog;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemProperties;
import android.provider.Settings;
import android.telephony.TelephonyManager;
import android.widget.Toast;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragment;
import androidx.preference.SwitchPreference;

import static com.yoshino.parts.Constants.*;

public class DeviceSettingsFragment extends PreferenceFragment implements Preference.OnPreferenceChangeListener {

    @Override
    public void onCreatePreferences(Bundle bundle, String key) {
        addPreferencesFromResource(R.xml.device_settings);

        SwitchPreference glovePref = findPreference(GLOVE_MODE);
        assert glovePref != null;
        glovePref.setChecked(Settings.System.getInt(glovePref.getContext().getContentResolver(), GLOVE_MODE, 0) == 1);
        glovePref.setOnPreferenceChangeListener(this);

        SwitchPreference smartStaminPref = findPreference(SMART_STAMINA_MODE);
        assert smartStaminPref != null;
        smartStaminPref.setChecked(Settings.System.getInt(smartStaminPref.getContext().getContentResolver(), SMART_STAMINA_MODE, 0) == 1);
        smartStaminPref.setOnPreferenceChangeListener(this);

        SwitchPreference notificationPref = findPreference(CS_NOTIFICATION);
        assert notificationPref != null;
        notificationPref.setChecked(Settings.System.getInt(notificationPref.getContext().getContentResolver(),
                CS_NOTIFICATION, 1) == 1);
        notificationPref.setOnPreferenceChangeListener(this);

        Preference statusPref = findPreference(CS_STATUS);
        assert statusPref != null;
        statusPref.setOnPreferenceClickListener(preference -> {
            preference.getContext().startActivity(new Intent()
                    .setClassName("com.sonymobile.customizationselector", "com.sonymobile.customizationselector.StatusActivity"));
            return true;
        });

        Preference logPref = findPreference(CS_LOG);
        assert logPref != null;
        logPref.setOnPreferenceClickListener(preference -> {
            preference.getContext().startActivity(new Intent()
                    .setClassName("com.sonymobile.customizationselector", "com.sonymobile.customizationselector.LogActivity"));
            return true;
        });

        Preference msActPref = findPreference(MODEM_SWITCHER);
        assert msActPref != null;
        msActPref.setOnPreferenceClickListener(preference -> {
            preference.getContext().startActivity(new Intent()
                    .setClassName("com.sonymobile.customizationselector", "com.sonymobile.customizationselector.ModemSwitcherActivity"));
            return true;
        });

        Preference slotPref = findPreference(NS_SLOT);
        SwitchPreference nsService = findPreference(NS_SERVICE);
        if (slotPref != null && nsService != null) {
            if (getContext().getSystemService(TelephonyManager.class).getActiveModemCount() > 1) {
                slotPref.setVisible(true);

                int slot = Settings.System.getInt(nsService.getContext().getContentResolver(), NS_SLOT, -1);
                slotPref.setSummary(slotPref.getContext().getString(R.string.sim_slot_summary) + (slot == -1 ? " INVALID" : " " + (slot + 1)));

                slotPref.setOnPreferenceClickListener(preference -> {
                    int nSlot = Settings.System.getInt(nsService.getContext().getContentResolver(), NS_SLOT, -1);
                    switch (nSlot) {
                        case 0:
                            Settings.System.putInt(nsService.getContext().getContentResolver(), NS_SLOT, 1);
                            slotPref.setSummary(slotPref.getContext().getString(R.string.sim_slot_summary) + " 2");
                            nsService.setEnabled(true);
                            break;
                        case 1:
                            Settings.System.putInt(nsService.getContext().getContentResolver(), NS_SLOT, -1);
                            slotPref.setSummary(slotPref.getContext().getString(R.string.sim_slot_summary) + " INVALID");
                            nsService.setEnabled(false);
                            break;
                        case -1:
                            Settings.System.putInt(nsService.getContext().getContentResolver(), NS_SLOT, 0);
                            slotPref.setSummary(slotPref.getContext().getString(R.string.sim_slot_summary) + " 1");
                            nsService.setEnabled(true);
                            break;
                    }
                    return true;
                });

                nsService.setEnabled(slot != -1);
            } else {
                slotPref.setVisible(false);
                nsService.setEnabled(true);
            }
            nsService.setChecked(Settings.System.getInt(nsService.getContext().getContentResolver(), NS_SERVICE, 0) == 1);
            nsService.setOnPreferenceChangeListener(this);
        }

        SwitchPreference imsPref = findPreference(CS_IMS);
        assert imsPref != null;
        if (Settings.System.getInt(imsPref.getContext().getContentResolver(), CS_IMS, 1) == 0) {
            imsPref.setChecked(false);
            notificationPref.setEnabled(false);
            msActPref.setEnabled(false);
        } else {
            imsPref.setChecked(true);
            notificationPref.setEnabled(true);
            msActPref.setEnabled(true);
        }
        imsPref.setOnPreferenceClickListener(preference -> {
            int ims = Settings.System.getInt(imsPref.getContext().getContentResolver(), CS_IMS, 1);
            if (ims == 1) {
                AlertDialog.Builder builder = new AlertDialog.Builder(imsPref.getContext());
                builder.setCancelable(false);
                builder.setTitle("Disable IMS features ?");
                builder.setMessage("You will lose all the carrier specific features such as VoLTE, VoWiFi and " +
                        "WiFi Calling; and Your device will switch to default modem config with basic mobile data feature.");
                builder.setPositiveButton("Disable", (dialogInterface, i) -> {
                    dialogInterface.dismiss();
                    Settings.System.putInt(imsPref.getContext().getContentResolver(), CS_IMS, 0);
                    imsPref.setChecked(false);
                    notificationPref.setEnabled(false);
                    msActPref.setEnabled(false);

                    sendBroadcast(preference.getContext(), 0);
                });
                builder.setNegativeButton("Cancel", (dialogInterface, i) -> {
                    dialogInterface.dismiss();
                    imsPref.setChecked(true);
                });
                builder.create().show();
            }
            if (ims == 0) {
                AlertDialog.Builder builder = new AlertDialog.Builder(imsPref.getContext());
                builder.setCancelable(false);
                builder.setTitle("Enable IMS features ?");
                builder.setMessage("This will allow you to use the provided carrier specific features such as VoLTE, " +
                        "VoWiFi and WiFi Calling; only if it worked on stock.\n\nYour device will prompt reboot if it " +
                        "found carrier specific modem.");
                builder.setPositiveButton("Enable", (dialogInterface, i) -> {
                    dialogInterface.dismiss();
                    Settings.System.putInt(imsPref.getContext().getContentResolver(), CS_IMS, 1);
                    imsPref.setChecked(true);
                    notificationPref.setEnabled(true);
                    msActPref.setEnabled(true);

                    sendBroadcast(preference.getContext(), 0);
                });
                builder.setNegativeButton("Cancel", (dialogInterface, i) -> {
                    dialogInterface.dismiss();
                    imsPref.setChecked(false);
                });
                builder.create().show();
            }
            return true;
        });

        SwitchPreference modemPref = findPreference(CS_RE_APPLY_MODEM);
        assert modemPref != null;
        modemPref.setChecked(Settings.System.getInt(modemPref.getContext().getContentResolver(), CS_RE_APPLY_MODEM, 0) == 1);
        modemPref.setOnPreferenceClickListener(preference -> {
            int applyModem = Settings.System.getInt(modemPref.getContext().getContentResolver(), CS_RE_APPLY_MODEM, 0);

            AlertDialog.Builder builder = new AlertDialog.Builder(modemPref.getContext());
            builder.setCancelable(false);
            builder.setTitle("Reboot required");
            builder.setMessage("A reboot is required to " + (applyModem == 0 ? "enable" : "disable") + " this feature. Are you sure you want to reboot ?");
            builder.setPositiveButton("Reboot", (dialogInterface, i) -> {
                dialogInterface.dismiss();
                Settings.System.putInt(modemPref.getContext().getContentResolver(), CS_RE_APPLY_MODEM, (applyModem ^ 1));
                modemPref.setChecked(applyModem == 0);

                sendBroadcast(preference.getContext(), 1);
            });
            builder.setNegativeButton("Cancel", (dialogInterface, i) -> {
                dialogInterface.dismiss();
                modemPref.setChecked(applyModem == 1);
            });
            builder.create().show();
            return true;
        });

        // The NFC controller selector (init.<device>-nfc.sh) exists on lilac (G8441 PN553 /
        // SO-02K FeliCa) and poplar (G8341/G8342 PN553 / SO-01K, SOV36, 701SO FeliCa).
        // Other yoshino devices have a single controller, so the choice is meaningless there.
        String vendorDevice = SystemProperties.get("ro.product.vendor.device", "");
        if (vendorDevice.startsWith("lilac") || vendorDevice.startsWith("poplar")) {
            ListPreference nfcPref = findPreference(NFC_VARIANT);
            assert nfcPref != null;
            nfcPref.setValue(SystemProperties.get(NFC_VARIANT_PROP, "auto"));
            nfcPref.setOnPreferenceChangeListener(this);
        } else {
            Preference nfcCategory = findPreference("nfc");
            if (nfcCategory != null) getPreferenceScreen().removePreference(nfcCategory);
        }

        ListPreference perfPref = findPreference(PERF_PROFILE);
        assert perfPref != null;
        perfPref.setValue(currentPerfProfile(perfPref));
        perfPref.setOnPreferenceChangeListener(this);
    }

    /*
     * The profile init is acting on, or the default when the property is unset
     * or holds something this build does not offer (for example a value left
     * behind by a different build). init has no trigger for such a value, so
     * the power HAL is on the default file and that is what gets shown.
     */
    private static String currentPerfProfile(ListPreference pref) {
        String value = SystemProperties.get(PERF_PROFILE_PROP, PERF_PROFILE_DEFAULT);
        return pref.findIndexOfValue(value) >= 0 ? value : PERF_PROFILE_DEFAULT;
    }

    /*
     * SystemProperties.set throws if the property service refuses the write.
     * Report that instead of crashing, so the caller can leave the list on the
     * profile that is still active.
     */
    private static boolean applyPerfProfile(Context context, String profile) {
        try {
            SystemProperties.set(PERF_PROFILE_PROP, profile);
            return true;
        } catch (RuntimeException e) {
            Toast.makeText(context, R.string.perf_profile_failed, Toast.LENGTH_LONG).show();
            return false;
        }
    }

    /*
     * Which NCI tuning file the stack should load for a given controller.
     * The Sony CXD224x config carries CXD224X_WORKAROUND, FELICA_CONFIG and
     * FeliCa HCI routing that would be wrong on the NXP part, so it ships under
     * its own name and is selected here rather than replacing libnfc-nci.conf —
     * /vendor/etc/libnfc-nci.conf already exists for the NXP HAL and outranks
     * /system/etc in the stack's search order.
     *
     * An empty value means "no override", which sends nfc_config.cc back to its
     * default libnfc-nci.conf.
     */
    static String nfcConfigFileFor(String variant) {
        return "felica".equals(variant) ? NFC_CFG_FELICA : "";
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object o) {
        switch (preference.getKey()) {
            case GLOVE_MODE:
                Settings.System.putInt(preference.getContext().getContentResolver(), GLOVE_MODE, (boolean) o ? 1 : 0);
                SystemProperties.set(GLOVE_PROP, (boolean) o ? "1" : "0");
                return true;
            case SMART_STAMINA_MODE:
                Settings.System.putInt(preference.getContext().getContentResolver(), SMART_STAMINA_MODE, (boolean) o ? 1 : 0);
                SystemProperties.set(SMART_STAMINA_PROP, (boolean) o ? "1" : "0");
                return true;
            case CS_NOTIFICATION:
                Settings.System.putInt(preference.getContext().getContentResolver(), CS_NOTIFICATION, (boolean) o ? 1 : 0);
                return true;
            case NS_SERVICE:
                Settings.System.putInt(preference.getContext().getContentResolver(), NS_SERVICE, (boolean) o ? 1 : 0);
                return true;
            case NFC_VARIANT: {
                String variant = (String) o;
                SystemProperties.set(NFC_VARIANT_PROP, variant);

                /*
                 * For an explicit choice the config file can be set straight
                 * away. For "auto" it is left to BootReceiver, which learns the
                 * answer from vendor.nfc.variant once the selector has run.
                 */
                if (!"auto".equals(variant)) {
                    SystemProperties.set(NFC_CFG_PROP, nfcConfigFileFor(variant));
                }

                /*
                 * Applied on the next boot rather than swapped live: the driver
                 * is a kernel module that has claimed the NFC IRQ and its GPIOs,
                 * and unloading it underneath a running HAL is a far worse
                 * failure than waiting for a restart.
                 *
                 * The reboot is left to the user — this app holds only
                 * RECEIVE_BOOT_COMPLETED and WRITE_SETTINGS, and adding
                 * android.permission.REBOOT to the privapp whitelist would be a
                 * real privilege increase for a convenience button.
                 */
                AlertDialog.Builder builder = new AlertDialog.Builder(preference.getContext());
                builder.setTitle("Reboot to apply");
                builder.setMessage("The NFC driver is chosen at boot, so this takes effect after you restart the phone.");
                builder.setPositiveButton("OK", (dialogInterface, i) -> dialogInterface.dismiss());
                builder.create().show();
                return true;
            }
            case PERF_PROFILE: {
                String profile = (String) o;
                ListPreference perfPref = (ListPreference) preference;
                if (perfPref.findIndexOfValue(profile) < 0) {
                    return false;
                }
                if (profile.equals(currentPerfProfile(perfPref))) {
                    // Already active. init runs property triggers even when a
                    // property is set to the value it already has, so writing
                    // it again would restart the power HAL for nothing.
                    return true;
                }
                if (!PERF_PROFILE_PERFORMANCE.equals(profile)) {
                    return applyPerfProfile(preference.getContext(), profile);
                }

                /*
                 * Performance is the only profile that raises a limit above
                 * the shipped default, so it is applied only after the user
                 * has read what it costs. Returning false keeps the list on
                 * its old value until then; cancelling changes nothing.
                 */
                AlertDialog.Builder builder = new AlertDialog.Builder(preference.getContext());
                builder.setTitle(R.string.perf_profile_performance_title);
                builder.setMessage(R.string.perf_profile_performance_warning);
                builder.setPositiveButton("Use Performance", (dialogInterface, i) -> {
                    dialogInterface.dismiss();
                    if (applyPerfProfile(preference.getContext(), profile)) {
                        perfPref.setValue(profile);
                    }
                });
                builder.setNegativeButton("Cancel", (dialogInterface, i) -> dialogInterface.dismiss());
                builder.create().show();
                return false;
            }
        }
        return false;
    }

    private void sendBroadcast(Context context, int pref) {
        Intent broadcast = new Intent();
        broadcast.putExtra("pref", pref);
        if (pref == 0) broadcast.putExtra(CS_IMS, Settings.System.getInt(context.getContentResolver(), CS_IMS, 1));
        if (pref == 1)
            broadcast.putExtra(CS_RE_APPLY_MODEM, Settings.System.getInt(context.getContentResolver(), CS_RE_APPLY_MODEM, 0));
        broadcast.addFlags(Intent.FLAG_INCLUDE_STOPPED_PACKAGES).setComponent(new ComponentName("com.sonymobile.customizationselector",
                "com.sonymobile.customizationselector.PreferenceReceiver"));
        context.sendBroadcast(broadcast);
    }
}
