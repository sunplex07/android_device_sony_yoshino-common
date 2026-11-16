/*
 * Cellular Debug — lilac.
 *
 * One place to see, exercise and capture the modem state that the 2026 bring-up
 * campaign showed actually matters:
 *   - ro.semc.version.cust.active EMPTY  => CustomizationSelector never applied a
 *     carrier customization. Combined with an EMPTY 3GPP profile-1 attach APN this
 *     is the failure where the modem decodes paging, never sends RRC, and the
 *     watchdog (dog_hb.c) kills the subsystem every ~60s.
 *   - profile-1 attach APN (e.g. fast.t-mobile.com) is NOT the data APN (e.g. tello).
 *   - rmnet_dataN carrying IMS is not the one carrying INTERNET.
 * Actions here perform REAL modem writes via the vendor helpers and log everything.
 */
package com.yoshino.parts;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.SystemProperties;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.File;
import java.io.InputStreamReader;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public class CellularDebugActivity extends Activity {

    private static final String PRIMER = "/vendor/bin/wds-attach-primer";
    private static final String RESTORE = "/vendor/bin/wds-profile-restore";
    private static final String WORKDIR = "/data/vendor/attach_primer";

    private TextView mOut;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        // Whole screen scrolls: buttons were clipping off the top on a 720x1280 panel.
        ScrollView scroller = new ScrollView(this);
        scroller.setFillViewport(true);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int p = (int) (12 * getResources().getDisplayMetrics().density);
        // This app's theme uses an overlay ActionBar, so content would otherwise be drawn
        // underneath it and the first button would be unreachable. Offset by its height.
        int bar = 0;
        android.util.TypedValue tv = new android.util.TypedValue();
        if (getTheme().resolveAttribute(android.R.attr.actionBarSize, tv, true)) {
            bar = android.util.TypedValue.complexToDimensionPixelSize(
                    tv.data, getResources().getDisplayMetrics());
        }
        if (bar <= 0) bar = (int) (56 * getResources().getDisplayMetrics().density);
        root.setPadding(p, p + bar, p, p);

        root.addView(button("Refresh status", v -> run("status")));
        root.addView(button("How to repair (root)", v -> howToRepair()));
        root.addView(button("Collect debug dump (zip)", v -> run("dump")));

        mOut = new TextView(this);
        mOut.setTypeface(android.graphics.Typeface.MONOSPACE);
        mOut.setTextSize(11);
        mOut.setTextIsSelectable(true);
        mOut.setFocusableInTouchMode(false);
        root.setDescendantFocusability(android.view.ViewGroup.FOCUS_BLOCK_DESCENDANTS);
        root.addView(mOut);
        scroller.addView(root);
        setContentView(scroller);
        run("status");
    }

    private Button button(String label, View.OnClickListener l) {
        Button b = new Button(this);
        b.setText(label);
        b.setAllCaps(false);
        b.setOnClickListener(l);
        return b;
    }

    private void confirm(String title, String msg, Runnable go) {
        new AlertDialog.Builder(this).setTitle(title).setMessage(msg)
                .setPositiveButton("Run", (d, w) -> go.run())
                .setNegativeButton("Cancel", null).show();
    }

    private void howToRepair() {
        new AlertDialog.Builder(this)
                .setTitle("Repairing the attach profile")
                .setMessage("A WIPED attach APN is repaired automatically by the boot primer from its saved snapshot - just reboot.\n\n"
                        + "Manual (root shell):\n"
                        + "  D=/data/vendor/attach_primer/m; mkdir -p $D; chmod 700 $D\n"
                        + "  /vendor/bin/wds-profile-restore $D <attach-apn>\n\n"
                        + "Use the ATTACH APN for your carrier (e.g. fast.t-mobile.com); it is not necessarily your data APN.")
                .setPositiveButton("OK", null).show();
    }

    private void toast(String s) {
        Toast.makeText(this, s, Toast.LENGTH_SHORT).show();
    }

    private void run(String what) {
        mOut.setText("working: " + what + " …\n");
        new Task().execute(what);
    }

    private class Task extends AsyncTask<String, Void, String> {
        @Override
        protected String doInBackground(String... a) {
            String what = a[0];
            if (what.equals("status")) return status();

            if (what.equals("dump")) return dump();
            return "?";
        }

        @Override
        protected void onPostExecute(String s) {
            mOut.setText(s);
        }
    }

    private String statusField(String key) {
        String all = readFile("/data/vendor/attach_primer/status.txt");
        for (String line : all.split("\n")) {
            int eq = line.indexOf('=');
            if (eq > 0 && line.substring(0, eq).equals(key)) return line.substring(eq + 1).trim();
        }
        return "";
    }

    private String readFile(String path) {
        StringBuilder b = new StringBuilder();
        try (BufferedReader r = new BufferedReader(new java.io.FileReader(path))) {
            String l;
            while ((l = r.readLine()) != null) b.append(l).append('\n');
        } catch (Exception ignored) { }
        return b.toString();
    }

    private String freshDir(String tag) {
        String d = WORKDIR + "/" + tag + "-" + System.currentTimeMillis();
        sh("mkdir -p " + d + " && chmod 700 " + d);
        return d;
    }

    private String status() {
        StringBuilder s = new StringBuilder();
        s.append("=== identity ===\n");
        s.append("baseband      : ").append(SystemProperties.get("gsm.version.baseband", "?")).append('\n');
        s.append("build         : ").append(SystemProperties.get("ro.build.date.utc", "?")).append('\n');
        String cust = SystemProperties.get("ro.semc.version.cust.active", "");
        // NOTE: empty here is NOT a fault. A device with full service + IMS was observed with
        // cust.active empty and fs unreadable. Informational only.
        s.append("cust.active   : ").append(cust.isEmpty() ? "<empty>" : cust)
         .append("   (informational, not a fault)\n");
        s.append("fs            : ").append(SystemProperties.get("ro.semc.version.fs", "?"))
         .append("   (informational)\n");
        s.append("\n=== radio ===\n");
        s.append("sim           : ").append(SystemProperties.get("gsm.sim.state", "?")).append('\n');
        s.append("operator      : ").append(SystemProperties.get("gsm.operator.alpha", "<none>")).append('\n');
        s.append("network       : ").append(SystemProperties.get("gsm.network.type", "?")).append('\n');
        s.append("\n=== modem watchdog ===\n");
        String dog = sh("dmesg | grep -c DOG_HB").trim();
        if (dog.contains("Permission") || dog.isEmpty()) {
            s.append("DOG_HB        : n/a (needs root; use Collect debug dump)\n");
        } else {
            s.append("DOG_HB        : ").append(dog).append('\n');
            s.append("modem SSR     : ")
             .append(sh("dmesg | grep -c 'Restart sequence for modem completed'").trim()).append('\n');
            s.append(sh("dmesg | grep DOG_HB | tail -3"));
        }
        s.append("\n=== attach profile (profile 1) ===\n");
        // Published by the root attach-primer at boot; this UI is unprivileged.
        String apn = SystemProperties.get("vendor.lilac.cell.apn", "");
        String snap = SystemProperties.get("vendor.lilac.cell.snapshot", "");
        String res = SystemProperties.get("vendor.lilac.cell.result", "");
        String up = SystemProperties.get("vendor.lilac.cell.uptime", "?");
        if (res.isEmpty()) {
            s.append("primer status : not published yet (primer has not run)\n");
        } else {
            s.append("attach APN    : ").append(apn.isEmpty()
                    ? "<EMPTY>  *** THIS IS THE FAULT: modem cannot attach ***" : apn)
             .append('\n');
            s.append("snapshot      : ").append(snap).append('\n');
            s.append("last result   : ").append(res).append("  (at ").append(up).append("s uptime)\n");
        }
        s.append("\n=== data paths ===\n");
        s.append(sh("dumpsys connectivity 2>/dev/null | grep -oE 'InterfaceName: rmnet_data[0-9]+' | sort -u"));
        s.append("\n(IMS and INTERNET use different rmnet interfaces - test the INTERNET one.)\n");
        return s.toString();
    }

    private String dump() {
        String stamp = new SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(new Date());
        // Stage inside our OWN dir: /data/vendor/attach_primer belongs to the primer's SELinux
        // domain and this app cannot write there. Android has tar, not zip.
        File dir = new File(getCacheDir(), "dump-" + stamp);
        dir.mkdirs();
        String d = dir.getAbsolutePath();
        String out = "/sdcard/lilac-cellular-" + stamp + ".tar.gz";
        StringBuilder s = new StringBuilder();

        sh("getprop > " + d + "/props.txt 2>&1");
        sh("getprop | grep -i lilac > " + d + "/lilac-props.txt 2>&1");
        // These need privileges we do not hold; they simply come out empty if denied.
        sh("logcat -b radio -d -v monotonic > " + d + "/radio.txt 2>&1");
        sh("dumpsys telephony.registry > " + d + "/telephony.txt 2>&1");
        sh("dumpsys connectivity > " + d + "/connectivity.txt 2>&1");

        StringBuilder notes = new StringBuilder();
        notes.append("lilac cellular debug dump ").append(stamp).append('\n');
        notes.append("attach APN : ").append(SystemProperties.get("vendor.lilac.cell.apn", "")).append('\n');
        notes.append("snapshot   : ").append(SystemProperties.get("vendor.lilac.cell.snapshot", "")).append('\n');
        notes.append("result     : ").append(SystemProperties.get("vendor.lilac.cell.result", "")).append('\n');
        notes.append('\n');
        notes.append("An EMPTY attach APN is the fault: the modem cannot compose an attach\n");
        notes.append("request, so it decodes paging, sends no RRC/NAS, is never rejected, and\n");
        notes.append("the watchdog restarts it. cust.active / fs are informational only.\n\n");
        notes.append("EXCLUDED ON PURPOSE: TA (sda1) and modemst1/2 - they carry IMEI, DRM keys\n");
        notes.append("and radio calibration. Never share those.\n\n");
        notes.append("radio/telephony/connectivity above are collected here. dmesg needs root;\n");
        notes.append("if any file is empty, collect it via adb instead:\n");
        notes.append("  adb shell su -c 'dmesg' > dmesg.txt\n");
        notes.append("  adb shell su -c 'logcat -b radio -d -v monotonic' > radio.txt\n");
        writeFile(d + "/README.txt", notes.toString());

        sh("tar czf " + out + " -C " + d + " . 2>&1");
        sh("chmod 644 " + out);
        s.append(notes).append('\n');
        s.append(sh("ls -la " + out));
        s.append(sh("tar tzf " + out + " 2>/dev/null"));
        return s.toString();
    }

    private void writeFile(String path, String text) {
        try (java.io.FileWriter w = new java.io.FileWriter(path)) {
            w.write(text);
        } catch (Exception ignored) { }
    }

    private String sh(String cmd) {
        StringBuilder out = new StringBuilder();
        try {
            Process p = new ProcessBuilder("/system/bin/sh", "-c", cmd).redirectErrorStream(true).start();
            try (BufferedReader r = new BufferedReader(new InputStreamReader(p.getInputStream()))) {
                String line;
                while ((line = r.readLine()) != null) out.append(line).append('\n');
            }
            p.waitFor();
        } catch (Exception e) {
            out.append("error: ").append(e).append('\n');
        }
        return out.toString();
    }
}
