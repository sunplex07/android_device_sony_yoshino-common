package com.sonymobile.customizationselector;

import java.nio.charset.StandardCharsets;
import java.util.Set;
import java.util.function.UnaryOperator;

/** Host test for ModemState. Run: tests/host/run.sh (plain javac/java, no Android). */
public final class ModemStateTest {
    static int failures = 0;
    static void check(String what, boolean ok) {
        System.out.println((ok ? "PASS " : "FAIL ") + what);
        if (!ok) failures++;
    }

    public static void main(String[] a) {
        String uk = "amss_fsg_poplar_telefonica_uk_tar.mbn";
        String gen = "amss_fsg_poplar_tar.mbn";
        String dsds = "amss_fsg_poplar_dsds_tmobile_us_ims_tar.mbn";
        Set<String> shipped = Set.of(uk, gen);
        UnaryOperator<String> link = s -> "default".equals(s) ? gen : s;

        // The bug: no status file, 2404 names an image this build does not ship.
        ModemState s = ModemState.resolve(null, dsds, link, shipped::contains);
        check("absent status + unshipped 2404 -> MISC_TA, not shipped",
                s.source == ModemState.Source.MISC_TA && dsds.equals(s.name) && !s.shipped);

        s = ModemState.resolve(null, uk, link, shipped::contains);
        check("absent status + shipped 2404 -> MISC_TA shipped",
                s.source == ModemState.Source.MISC_TA && uk.equals(s.name) && s.shipped);

        s = ModemState.resolve("0," + uk, gen, link, shipped::contains);
        check("good status wins over 2404", s.source == ModemState.Source.STATUS_FILE && uk.equals(s.name));

        s = ModemState.resolve("0,default", null, link, shipped::contains);
        check("status 'default' resolved through symlink", gen.equals(s.name) && s.shipped);

        s = ModemState.resolve("2,default", uk, link, shipped::contains);
        check("failed status (2) ignored, falls back to 2404", s.source == ModemState.Source.MISC_TA && uk.equals(s.name));

        s = ModemState.resolve("0," + dsds, uk, link, shipped::contains);
        check("status naming unshipped image defers to shipped 2404", uk.equals(s.name) && s.shipped);

        s = ModemState.resolve(null, "temporary_modem", link, shipped::contains);
        check("temporary_modem -> pending", ModemState.TEMPORARY_MODEM.equals(s.name) && !s.shipped);

        s = ModemState.resolve(null, null, link, shipped::contains);
        check("nothing readable -> UNKNOWN (no crash)", s == ModemState.UNKNOWN);

        s = ModemState.resolve("garbage", "../../etc/passwd", link, shipped::contains);
        check("unsafe 2404 rejected", s == ModemState.UNKNOWN);

        s = ModemState.resolve("0,amss_fsg_x_tar.mbn,extra", null, link, shipped::contains);
        check("malformed status rejected", s == ModemState.UNKNOWN);

        check("taText strips trailing NUL",
                dsds.equals(ModemState.taText((dsds + "\0\0").getBytes(StandardCharsets.UTF_8))));
        check("taText rejects embedded NUL", ModemState.taText("amss\0fsg".getBytes(StandardCharsets.UTF_8)) == null);
        check("taText empty -> null", ModemState.taText(new byte[0]) == null && ModemState.taText(null) == null);

        check("isImageName mirrors guard: rejects short/odd",
                !ModemState.isImageName("amss_fsg_tar.mbn") && !ModemState.isImageName("amss_fsg_a b_tar.mbn")
                && !ModemState.isImageName("amss_fsg_..x_tar.mbn") && ModemState.isImageName(dsds));

        System.out.println(failures == 0 ? "ALL PASS" : failures + " FAILED");
        System.exit(failures == 0 ? 0 : 1);
    }
}
