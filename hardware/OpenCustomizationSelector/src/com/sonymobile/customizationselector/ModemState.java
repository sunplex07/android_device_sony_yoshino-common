package com.sonymobile.customizationselector;

import java.util.function.Predicate;
import java.util.function.UnaryOperator;

/**
 * Which modem configuration the Modem Switcher screen should report as current.
 *
 * Pure logic, no Android dependencies, so it is host-tested
 * (tests/host/ModemStateTest.java).
 *
 * Sony's switcher only writes /cache/modem/modem_switcher_status after it has
 * actually run. modem-switcher-guard deliberately does not run it when MiscTA
 * unit 2404 names an image this build does not ship (e.g. a dual-SIM or
 * carrier fsg left behind by a different firmware). The status file is then
 * absent, and the screen used to give up entirely, so the user could not
 * pick a shipped configuration to recover. Here the status file is preferred
 * (it is what the last switcher run verified), TA 2404 is the fallback (it is
 * what the switcher will act on), and failing both we report "unknown"
 * instead of refusing to open.
 */
final class ModemState {

    enum Source { STATUS_FILE, MISC_TA, NONE }

    static final String TEMPORARY_MODEM = "temporary_modem";

    final Source source;
    /** Image basename, TEMPORARY_MODEM, or null when nothing is known. */
    final String name;
    /** True when {@link #name} is an image shipped in this build. */
    final boolean shipped;

    private ModemState(Source source, String name, boolean shipped) {
        this.source = source;
        this.name = name;
        this.shipped = shipped;
    }

    static final ModemState UNKNOWN = new ModemState(Source.NONE, null, false);

    /** Same rule as modem-guard's image_name(): a plain amss_fsg_*_tar.mbn basename. */
    static boolean isImageName(String s) {
        if (s == null || s.length() < 18 || s.length() > 240
                || !s.startsWith("amss_fsg_") || !s.endsWith("_tar.mbn") || s.contains("..")) {
            return false;
        }
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            boolean ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                    || c == '_' || c == '-' || c == '.';
            if (!ok) return false;
        }
        return true;
    }

    /** Stock status line "<status>,<config>". Returns config only for status 0, else null. */
    static String successfulStatusName(String line) {
        if (line == null) return null;
        String[] v = line.trim().split(",", -1);
        if (v.length != 2 || !"0".equals(v[0].trim())) return null;
        String name = v[1].trim();
        return name.isEmpty() ? null : name;
    }

    /** MiscTA unit bytes as text: trailing NULs dropped; embedded NUL or empty -> null. */
    static String taText(byte[] unit) {
        if (unit == null) return null;
        int n = unit.length;
        while (n > 0 && unit[n - 1] == 0) n--;
        if (n == 0) return null;
        for (int i = 0; i < n; i++) {
            if (unit[i] == 0) return null;
        }
        return new String(unit, 0, n, java.nio.charset.StandardCharsets.UTF_8);
    }

    /**
     * @param statusLine    first line of modem_switcher_status, or null if absent/unreadable
     * @param taCurrent     MiscTA 2404 as text, or null if unreadable
     * @param resolveLink   maps a status-file name through the modem dir's symlinks
     *                      ("default" -> its target); identity when not a link
     * @param isShipped     whether an image basename exists in this build's modem dir
     */
    static ModemState resolve(String statusLine, String taCurrent,
            UnaryOperator<String> resolveLink, Predicate<String> isShipped) {
        String fromStatus = successfulStatusName(statusLine);
        String resolved = fromStatus == null ? null : resolveLink.apply(fromStatus);
        if (isImageName(resolved) && isShipped.test(resolved)) {
            return new ModemState(Source.STATUS_FILE, resolved, true);
        }
        if (isImageName(taCurrent)) {
            return new ModemState(Source.MISC_TA, taCurrent, isShipped.test(taCurrent));
        }
        if (TEMPORARY_MODEM.equals(taCurrent)) {
            // A switch was requested but has not been applied (or verified) yet.
            return new ModemState(Source.MISC_TA, TEMPORARY_MODEM, false);
        }
        if (isImageName(resolved)) {
            return new ModemState(Source.STATUS_FILE, resolved, false);
        }
        return UNKNOWN;
    }
}
