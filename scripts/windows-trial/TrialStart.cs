using System;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;

internal class TrialStart
{
    [STAThread]
    private static int Main()
    {
        try
        {
            string app = AppDomain.CurrentDomain.BaseDirectory;
            string root = Directory.GetParent(app.TrimEnd(Path.DirectorySeparatorChar)).FullName;
            string data = Path.Combine(root, "試用データ");
            string exe = Path.Combine(app, "secondlife-bin.exe");
            if (FileVersionInfo.GetVersionInfo(exe).FileVersion != "26.4.0.8")
                throw new IOException("26.4.0.8のViewer本体を確認できません。ZIPをすべて展開してください。");
            string roaming = Path.Combine(data, "roaming");
            string local = Path.Combine(data, "local");
            string cache = Path.Combine(local, "SecondLife");
            string chat = Path.Combine(data, "chat_logs");
            string temp = Path.Combine(data, "temp");
            foreach (string dir in new[] { Path.Combine(roaming,"SecondLife","user_settings"), cache, chat, temp })
            {
                Directory.CreateDirectory(dir);
                using (File.Create(Path.Combine(dir, Guid.NewGuid().ToString("N") + ".tmp"),
                                   1, FileOptions.DeleteOnClose)) {}
            }
            using (var gate = new FileStream(Path.Combine(data,"trial.lock"), FileMode.OpenOrCreate,
                                             FileAccess.ReadWrite, FileShare.None))
            {
                var start = new ProcessStartInfo(exe);
                start.WorkingDirectory = app;
                start.UseShellExecute = false;
                start.EnvironmentVariables["APPDATA"] = roaming;
                start.EnvironmentVariables["LOCALAPPDATA"] = local;
                start.EnvironmentVariables["TEMP"] = temp;
                start.EnvironmentVariables["TMP"] = temp;
                start.EnvironmentVariables["PRELOG"] = Path.Combine(data,"initial-paths.log");
                start.EnvironmentVariables["PJ0225_FACE_TRACE"] = "0";
                start.Arguments = "--settings settings_dual_view.xml --skipupdatecheck --disablecrashlogger " +
                    "--set AutoLogin false --set MigrateCacheDirectory false --set SLURLPassToOtherInstance false " +
                    "--set ShowConsoleWindow false --set QAMode true --set Language ja " +
                    "--set CacheLocation \"" + cache + "\" --set NewCacheLocation \"" + cache + "\" " +
                    "--set PerAccount.InstantMessageLogPath \"" + chat + "\"";
                using (Process viewer = Process.Start(start))
                {
                    viewer.WaitForExit();
                    return viewer.ExitCode;
                }
            }
        }
        catch (Exception ex)
        {
            MessageBox.Show("試用版を起動できません。すでに起動中の場合は同じ画面をお使いください。" +
                            "\n書き込み可能なフォルダーにZIPをすべて展開したか確認してください。\n\n" + ex.Message,
                            "SnowPastel Multi-View Viewer", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }
    }
}
