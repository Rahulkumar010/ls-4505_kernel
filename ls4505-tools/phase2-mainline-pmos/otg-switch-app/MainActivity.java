package com.ls4505.otg;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.BufferedReader;
import java.io.InputStreamReader;

public class MainActivity extends Activity implements View.OnClickListener {
    private TextView statusView;

    private String runRoot(String cmd) {
        StringBuilder sb = new StringBuilder();
        try {
            Process p = new ProcessBuilder("su", "-c", cmd).redirectErrorStream(true).start();
            BufferedReader br = new BufferedReader(new InputStreamReader(p.getInputStream()));
            String line;
            while ((line = br.readLine()) != null) {
                sb.append(line).append("\n");
            }
            p.waitFor();
        } catch (Exception e) {
            sb.append("Error: ").append(e.getMessage());
        }
        return sb.toString().trim();
    }

    private void refreshStatus(String prefix) {
        String out = runRoot("/system/xbin/otg-mode status; for d in /sys/bus/usb/devices/*; do [ -e $d/idVendor ] && echo \"USB $(basename $d): $(cat $d/idVendor):$(cat $d/idProduct) $(cat $d/product 2>/dev/null)\"; done");
        statusView.setText((prefix.isEmpty() ? "" : prefix + "\n\n") + out);
    }

    private Button makeBtn(String text, int bgColor, int id) {
        Button b = new Button(this);
        b.setId(id);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextSize(16f);
        b.setTextColor(Color.WHITE);
        b.setBackgroundColor(bgColor);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.setMargins(0, 0, 0, 24);
        b.setLayoutParams(lp);
        b.setPadding(24, 28, 24, 28);
        b.setOnClickListener(this);
        return b;
    }

    @Override
    public void onClick(View v) {
        int id = v.getId();
        if (id == 1) {
            String r = runRoot("/system/xbin/otg-mode host && sleep 2");
            refreshStatus(r);
        } else if (id == 2) {
            String r = runRoot("/system/xbin/otg-mode peripheral");
            refreshStatus(r);
        } else if (id == 3) {
            refreshStatus("");
        } else if (id == 4) {
            runRoot("/system/xbin/boot-linux");
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.parseColor("#0D1117"));
        root.setPadding(32, 40, 32, 32);

        TextView title = new TextView(this);
        title.setText("LS-4505 OTG 5V & Dual-Boot");
        title.setTextSize(20f);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setTextColor(Color.WHITE);
        title.setGravity(Gravity.CENTER_HORIZONTAL);
        title.setPadding(0, 0, 0, 32);
        root.addView(title);

        root.addView(makeBtn("Enable USB OTG Host (5V Boost ON)", Color.parseColor("#238636"), 1));
        root.addView(makeBtn("Disable OTG / PC ADB & Charge Mode", Color.parseColor("#1F6FEB"), 2));
        root.addView(makeBtn("Refresh Connected USB Devices", Color.parseColor("#30363D"), 3));
        root.addView(makeBtn("Reboot -> Mainline Linux 6.12.1 (JWM)", Color.parseColor("#8957E5"), 4));

        statusView = new TextView(this);
        statusView.setTextSize(13f);
        statusView.setTypeface(Typeface.MONOSPACE);
        statusView.setTextColor(Color.parseColor("#E6EDF3"));
        statusView.setBackgroundColor(Color.parseColor("#161B22"));
        statusView.setPadding(24, 24, 24, 24);

        ScrollView sv = new ScrollView(this);
        sv.addView(statusView);
        root.addView(sv);

        setContentView(root);
        refreshStatus("");
    }
}