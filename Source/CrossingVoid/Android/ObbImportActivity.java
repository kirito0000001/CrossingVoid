package com.tfac.crossingvoid;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.StatFs;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.regex.Pattern;

public class ObbImportActivity extends Activity {
    private static final String LAUNCHER_PACKAGE = "com.TFAC.CorssingVoidLauncher";
    private static final String LEGACY_LAUNCHER_PACKAGE = "com.lingjing.launcher.android";
    private static final String RESULT_RECEIVER = "com.lingjing.launcher.android.ObbInstalledReceiver";
    private static final String RESULT_ACTION = "com.lingjing.launcher.android.action.OBB_INSTALLED";
    private static final String RESULT_FAILURE_ACTION = "com.lingjing.launcher.android.action.OBB_INSTALL_FAILED";
    private static final Pattern OBB_NAME = Pattern.compile("^(main|patch)\\.\\d+\\.com\\.TFAC\\.CorssingVoid\\.obb$");

    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private ProgressBar progressBar;
    private TextView statusText;
    private long lastUiUpdate;
    private String resultLauncherPackage = LAUNCHER_PACKAGE;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        buildUi();

        Uri source = getIntent().getData();
        String sourceLauncherPackage = launcherPackageForAuthority(source == null ? null : source.getAuthority());
        String fileName = getIntent().getStringExtra("fileName");
        String installToken = getIntent().getStringExtra("installToken");
        long expectedSize = getIntent().getLongExtra("sizeBytes", 0L);
        if (
            source == null ||
            !"content".equals(source.getScheme()) ||
            sourceLauncherPackage == null ||
            fileName == null ||
            !OBB_NAME.matcher(fileName).matches() ||
            installToken == null ||
            installToken.isEmpty() ||
            expectedSize <= 0
        ) {
            showError("游戏资源安装请求无效。");
            return;
        }
        resultLauncherPackage = sourceLauncherPackage;

        executor.execute(() -> importObb(source, fileName, installToken, expectedSize));
    }

    @Override
    protected void onDestroy() {
        executor.shutdownNow();
        super.onDestroy();
    }

    private void buildUi() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setPadding(80, 52, 80, 52);
        root.setBackgroundColor(Color.rgb(9, 14, 20));

        TextView title = new TextView(this);
        title.setText("零境交错：空界幻境");
        title.setTextColor(Color.rgb(255, 226, 165));
        title.setTextSize(28);
        title.setGravity(Gravity.CENTER);
        root.addView(title, new LinearLayout.LayoutParams(-1, -2));

        statusText = new TextView(this);
        statusText.setText("正在安装游戏资源");
        statusText.setTextColor(Color.rgb(210, 214, 218));
        statusText.setTextSize(16);
        statusText.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams statusParams = new LinearLayout.LayoutParams(-1, -2);
        statusParams.setMargins(0, 26, 0, 20);
        root.addView(statusText, statusParams);

        progressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progressBar.setMax(1000);
        progressBar.setProgress(0);
        root.addView(progressBar, new LinearLayout.LayoutParams(-1, 24));
        setContentView(root);
    }

    private void importObb(Uri source, String fileName, String installToken, long expectedSize) {
        File temporary = null;
        try {
            File obbDirectory = getObbDir();
            if (!obbDirectory.exists() && !obbDirectory.mkdirs()) {
                throw new IllegalStateException("无法创建游戏资源目录。");
            }
            long available = new StatFs(obbDirectory.getAbsolutePath()).getAvailableBytes();
            if (available < expectedSize + 64L * 1024L * 1024L) {
                throw new IllegalStateException("存储空间不足，无法安装 OBB 资源。");
            }

            File target = new File(obbDirectory, fileName);
            temporary = new File(obbDirectory, fileName + ".installing");
            if (temporary.exists()) {
                temporary.delete();
            }

            byte[] buffer = new byte[256 * 1024];
            long copied = 0L;
            try (
                InputStream input = new BufferedInputStream(getContentResolver().openInputStream(source), buffer.length);
                OutputStream output = new BufferedOutputStream(new FileOutputStream(temporary), buffer.length)
            ) {
                int read;
                while ((read = input.read(buffer)) >= 0) {
                    if (Thread.currentThread().isInterrupted()) {
                        throw new InterruptedException("资源安装已中断。");
                    }
                    if (read == 0) {
                        continue;
                    }
                    output.write(buffer, 0, read);
                    copied += read;
                    updateProgress(copied, expectedSize);
                }
            }
            if (temporary.length() != expectedSize) {
                throw new IllegalStateException("OBB 资源大小不正确。");
            }
            if (target.exists() && !target.delete()) {
                throw new IllegalStateException("无法替换旧版 OBB 资源。");
            }
            if (!temporary.renameTo(target)) {
                throw new IllegalStateException("无法保存 OBB 资源。");
            }

            runOnUiThread(() -> {
                progressBar.setProgress(1000);
                statusText.setText("游戏资源安装完成");
            });
            notifyLauncher(installToken, fileName);
            Thread.sleep(450L);
            Intent launch = getPackageManager().getLaunchIntentForPackage(getPackageName());
            if (launch != null) {
                launch.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);
                startActivity(launch);
            }
            finish();
        } catch (Exception error) {
            if (temporary != null) {
                temporary.delete();
            }
            String message = error.getMessage() == null ? "游戏资源安装失败。" : error.getMessage();
            notifyLauncherFailure(installToken, message);
            runOnUiThread(() -> showError(message));
        }
    }

    private void updateProgress(long copied, long total) {
        long now = System.currentTimeMillis();
        if (now - lastUiUpdate < 180L && copied < total) {
            return;
        }
        lastUiUpdate = now;
        int progress = (int) Math.min(1000L, copied * 1000L / Math.max(1L, total));
        String text = String.format(Locale.ROOT, "正在安装游戏资源  %d%%", progress / 10);
        runOnUiThread(() -> {
            progressBar.setProgress(progress);
            statusText.setText(text);
        });
    }

    private void notifyLauncher(String installToken, String fileName) {
        Intent result = new Intent(RESULT_ACTION);
        result.setClassName(resultLauncherPackage, RESULT_RECEIVER);
        result.putExtra("installToken", installToken);
        result.putExtra("fileName", fileName);
        sendBroadcast(result);
    }

    private void notifyLauncherFailure(String installToken, String message) {
        Intent result = new Intent(RESULT_FAILURE_ACTION);
        result.setClassName(resultLauncherPackage, RESULT_RECEIVER);
        result.putExtra("installToken", installToken);
        result.putExtra("errorMessage", message);
        sendBroadcast(result);
    }

    private void showError(String message) {
        statusText.setText(message);
        statusText.setTextColor(Color.rgb(255, 132, 132));
        progressBar.setVisibility(View.GONE);

        Button back = new Button(this);
        back.setText("返回零境启动器");
        back.setOnClickListener(view -> {
            Intent intent = getPackageManager().getLaunchIntentForPackage(resultLauncherPackage);
            if (intent != null) {
                startActivity(intent);
            }
            finish();
        });
        LinearLayout root = (LinearLayout) statusText.getParent();
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(-1, -2);
        params.setMargins(0, 24, 0, 0);
        root.addView(back, params);
    }

    private static String launcherPackageForAuthority(String authority) {
        if ((LAUNCHER_PACKAGE + ".fileprovider").equals(authority)) {
            return LAUNCHER_PACKAGE;
        }
        if ((LEGACY_LAUNCHER_PACKAGE + ".fileprovider").equals(authority)) {
            return LEGACY_LAUNCHER_PACKAGE;
        }
        return null;
    }
}
