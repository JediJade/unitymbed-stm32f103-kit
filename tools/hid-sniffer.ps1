<#
.SYNOPSIS
  HID sniffer - shows how Windows parsed the report descriptor, and captures
  live reports coming out of the device.
.DESCRIPTION
  Windows binds keyboard and mouse collections to its own class drivers, so a
  raw read on the HID handle is refused. Two things still work:

    -Descriptor : opens each collection with zero access rights and asks the
                  HID parser what it found (usage, report IDs, report sizes).
    -Live       : registers for Raw Input with RIDEV_INPUTSINK, which delivers
                  every report the device sends even while another window has
                  focus, tagged with the originating device.
.EXAMPLE
  .\hid-sniffer.ps1                              # descriptor of VID_0483&PID_5751
  .\hid-sniffer.ps1 -Live -Seconds 12
  .\hid-sniffer.ps1 -Live -Seconds 12 -Log hid.log
#>
[CmdletBinding()]
param(
    [string]$Match = 'VID_0483&PID_5751',
    [switch]$Live,
    [int]$Seconds = 10,
    [string]$Log
)

$ErrorActionPreference = 'Stop'

function Write-Line([string]$text)
{
    Write-Output $text
    if ($Log) { Add-Content -Path $Log -Value $text -Encoding utf8 }
}

# --- HID parser interop -----------------------------------------------------
if (-not ('HidPeek' -as [type]))
{
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class HidPeek
{
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern IntPtr CreateFileW(string name, uint access, uint share,
        IntPtr sec, uint creation, uint flags, IntPtr template);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool CloseHandle(IntPtr handle);

    [DllImport("hid.dll", SetLastError = true)]
    static extern bool HidD_GetAttributes(IntPtr handle, ref HIDD_ATTRIBUTES attr);

    [DllImport("hid.dll", SetLastError = true)]
    static extern bool HidD_GetPreparsedData(IntPtr handle, out IntPtr preparsed);

    [DllImport("hid.dll", SetLastError = true)]
    static extern bool HidD_FreePreparsedData(IntPtr preparsed);

    [DllImport("hid.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern bool HidD_GetProductString(IntPtr handle, StringBuilder buffer, int length);

    [DllImport("hid.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern bool HidD_GetManufacturerString(IntPtr handle, StringBuilder buffer, int length);

    [DllImport("hid.dll", SetLastError = true)]
    static extern int HidP_GetCaps(IntPtr preparsed, ref HIDP_CAPS caps);

    [StructLayout(LayoutKind.Sequential)]
    struct HIDD_ATTRIBUTES
    {
        public int Size;
        public ushort VendorID;
        public ushort ProductID;
        public ushort VersionNumber;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct HIDP_CAPS
    {
        public ushort Usage;
        public ushort UsagePage;
        public ushort InputReportByteLength;
        public ushort OutputReportByteLength;
        public ushort FeatureReportByteLength;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 17)]
        public ushort[] Reserved;
        public ushort NumberLinkCollectionNodes;
        public ushort NumberInputButtonCaps;
        public ushort NumberInputValueCaps;
        public ushort NumberInputDataIndices;
        public ushort NumberOutputButtonCaps;
        public ushort NumberOutputValueCaps;
        public ushort NumberOutputDataIndices;
        public ushort NumberFeatureButtonCaps;
        public ushort NumberFeatureValueCaps;
        public ushort NumberFeatureDataIndices;
    }

    public class Info
    {
        public string Path;
        public bool   Opened;
        public string Error;
        public ushort Vid, Pid, Version;
        public string Product, Manufacturer;
        public ushort UsagePage, Usage;
        public ushort InputLen, OutputLen, FeatureLen;
        public ushort LinkCollections, InputButtonCaps, InputValueCaps;
    }

    public static Info Query(string path)
    {
        Info info = new Info();
        info.Path = path;

        // Zero access rights: enough for the parser, allowed on collections
        // that Windows keeps open for itself.
        IntPtr h = CreateFileW(path, 0, 3 /* share RW */, IntPtr.Zero,
                               3 /* OPEN_EXISTING */, 0x40000000 /* OVERLAPPED */, IntPtr.Zero);

        if (h == new IntPtr(-1))
        {
            info.Error = "CreateFile failed, win32 error " + Marshal.GetLastWin32Error();
            return info;
        }

        try
        {
            HIDD_ATTRIBUTES attr = new HIDD_ATTRIBUTES();
            attr.Size = Marshal.SizeOf(typeof(HIDD_ATTRIBUTES));
            if (HidD_GetAttributes(h, ref attr))
            {
                info.Vid = attr.VendorID;
                info.Pid = attr.ProductID;
                info.Version = attr.VersionNumber;
            }

            StringBuilder sb = new StringBuilder(256);
            if (HidD_GetProductString(h, sb, sb.Capacity * 2)) { info.Product = sb.ToString(); }
            sb.Length = 0;
            if (HidD_GetManufacturerString(h, sb, sb.Capacity * 2)) { info.Manufacturer = sb.ToString(); }

            IntPtr pre;
            if (!HidD_GetPreparsedData(h, out pre))
            {
                info.Error = "HidD_GetPreparsedData failed, win32 error " + Marshal.GetLastWin32Error();
                return info;
            }

            try
            {
                HIDP_CAPS caps = new HIDP_CAPS();
                int status = HidP_GetCaps(pre, ref caps);
                if (status != 0x00110000 /* HIDP_STATUS_SUCCESS */)
                {
                    info.Error = "HidP_GetCaps returned 0x" + status.ToString("X8");
                    return info;
                }

                info.UsagePage       = caps.UsagePage;
                info.Usage           = caps.Usage;
                info.InputLen        = caps.InputReportByteLength;
                info.OutputLen       = caps.OutputReportByteLength;
                info.FeatureLen      = caps.FeatureReportByteLength;
                info.LinkCollections = caps.NumberLinkCollectionNodes;
                info.InputButtonCaps = caps.NumberInputButtonCaps;
                info.InputValueCaps  = caps.NumberInputValueCaps;
                info.Opened          = true;
            }
            finally
            {
                HidD_FreePreparsedData(pre);
            }
        }
        finally
        {
            CloseHandle(h);
        }

        return info;
    }
}
'@
}

# --- Raw Input capture ------------------------------------------------------
if (-not ('RawInputTap' -as [type]))
{
    Add-Type -ReferencedAssemblies System.Windows.Forms, System.Drawing -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

public class RawInputTap : Form
{
    const int WM_INPUT = 0x00FF;
    const uint RIDEV_INPUTSINK = 0x00000100;
    const uint RID_INPUT = 0x10000003;
    const uint RIDI_DEVICENAME = 0x20000007;

    [StructLayout(LayoutKind.Sequential)]
    struct RAWINPUTDEVICE
    {
        public ushort UsagePage;
        public ushort Usage;
        public uint   Flags;
        public IntPtr Target;
    }

    [DllImport("user32.dll", SetLastError = true)]
    static extern bool RegisterRawInputDevices(RAWINPUTDEVICE[] devices, uint count, uint size);

    [DllImport("user32.dll", SetLastError = true)]
    static extern uint GetRawInputData(IntPtr input, uint command, byte[] data,
                                       ref uint size, uint headerSize);

    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern uint GetRawInputDeviceInfoW(IntPtr device, uint command,
                                              StringBuilder data, ref uint size);

    readonly string filter;
    readonly Dictionary<IntPtr, string> names = new Dictionary<IntPtr, string>();

    public List<string> Reports = new List<string>();
    public int Skipped;

    public RawInputTap(string deviceFilter, int seconds)
    {
        filter = deviceFilter.ToUpperInvariant();

        // Message-only intent: never show, just own a handle for WM_INPUT.
        ShowInTaskbar = false;
        WindowState   = FormWindowState.Minimized;
        Opacity       = 0;

        Timer stop = new Timer();
        stop.Interval = seconds * 1000;
        stop.Tick += delegate { stop.Stop(); Close(); };

        Load += delegate
        {
            RAWINPUTDEVICE[] rid = new RAWINPUTDEVICE[2];
            rid[0].UsagePage = 0x01; rid[0].Usage = 0x02;  /* mouse    */
            rid[0].Flags = RIDEV_INPUTSINK; rid[0].Target = Handle;
            rid[1].UsagePage = 0x01; rid[1].Usage = 0x06;  /* keyboard */
            rid[1].Flags = RIDEV_INPUTSINK; rid[1].Target = Handle;

            if (!RegisterRawInputDevices(rid, 2, (uint)Marshal.SizeOf(typeof(RAWINPUTDEVICE))))
            {
                Reports.Add("RegisterRawInputDevices failed, win32 error "
                           + Marshal.GetLastWin32Error());
            }
            stop.Start();
        };
    }

    string DeviceName(IntPtr device)
    {
        string cached;
        if (names.TryGetValue(device, out cached)) { return cached; }

        uint size = 0;
        GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, null, ref size);

        string name = "";
        if (size > 0)
        {
            StringBuilder sb = new StringBuilder((int)size + 1);
            if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, sb, ref size) != uint.MaxValue)
            {
                name = sb.ToString();
            }
        }

        names[device] = name;
        return name;
    }

    protected override void WndProc(ref Message m)
    {
        if (m.Msg == WM_INPUT) { Handle_WM_INPUT(m.LParam); }
        base.WndProc(ref m);
    }

    void Handle_WM_INPUT(IntPtr input)
    {
        uint headerSize = (uint)(IntPtr.Size == 8 ? 24 : 16);
        uint size = 0;
        GetRawInputData(input, RID_INPUT, null, ref size, headerSize);
        if (size == 0) { return; }

        byte[] buf = new byte[size];
        if (GetRawInputData(input, RID_INPUT, buf, ref size, headerSize) != size) { return; }

        uint   type   = BitConverter.ToUInt32(buf, 0);
        IntPtr device = (IntPtr.Size == 8)
                        ? new IntPtr(BitConverter.ToInt64(buf, 8))
                        : new IntPtr(BitConverter.ToInt32(buf, 8));

        string name = DeviceName(device);
        if (name.ToUpperInvariant().IndexOf(filter) < 0) { Skipped++; return; }

        int body = (int)headerSize;
        string stamp = DateTime.Now.ToString("HH:mm:ss.fff");

        if (type == 0) /* RIM_TYPEMOUSE */
        {
            ushort flags     = BitConverter.ToUInt16(buf, body + 0);
            ushort btnFlags  = BitConverter.ToUInt16(buf, body + 4);
            ushort btnData   = BitConverter.ToUInt16(buf, body + 6);
            int    lastX     = BitConverter.ToInt32(buf, body + 12);
            int    lastY     = BitConverter.ToInt32(buf, body + 16);

            Reports.Add(string.Format(
                "{0}  MOUSE   dX={1,5} dY={2,5} buttonFlags=0x{3:X4} wheel={4,6} flags=0x{5:X4}",
                stamp, lastX, lastY, btnFlags, (short)btnData, flags));
        }
        else if (type == 1) /* RIM_TYPEKEYBOARD */
        {
            ushort makeCode = BitConverter.ToUInt16(buf, body + 0);
            ushort kbdFlags = BitConverter.ToUInt16(buf, body + 2);
            ushort vkey     = BitConverter.ToUInt16(buf, body + 6);

            Reports.Add(string.Format(
                "{0}  KEYBD   makeCode=0x{1:X2} vKey=0x{2:X2} flags=0x{3:X4} ({4})",
                stamp, makeCode, vkey, kbdFlags,
                (kbdFlags & 1) == 0 ? "down" : "up"));
        }
        else
        {
            Reports.Add(string.Format("{0}  HID     {1} bytes", stamp, size - headerSize));
        }
    }

    public static RawInputTap Run(string deviceFilter, int seconds)
    {
        RawInputTap tap = new RawInputTap(deviceFilter, seconds);
        Application.Run(tap);
        return tap;
    }
}
'@
}

# --- descriptor mode --------------------------------------------------------
function Show-Descriptor
{
    $collections = Get-PnpDevice -PresentOnly |
        Where-Object { $_.InstanceId -like "HID\$Match*" } |
        Sort-Object InstanceId

    if (-not $collections)
    {
        Write-Line "No HID collection matching '$Match' is present."
        return
    }

    foreach ($c in $collections)
    {
        # HID interface path is the instance id with '\' -> '#', plus the
        # HID device interface class GUID.
        $path = '\\?\' + ($c.InstanceId -replace '\\', '#') +
                '#{4d1e55b2-f16f-11cf-88cb-001111000030}'

        $info = [HidPeek]::Query($path)

        Write-Line ''
        Write-Line "=== $($c.FriendlyName)  [$($c.Class)] ==="
        Write-Line "  instance : $($c.InstanceId)"

        if (-not $info.Opened)
        {
            Write-Line "  FAILED   : $($info.Error)"
            continue
        }

        Write-Line ("  ids      : VID_{0:X4} PID_{1:X4} rev {2:X4}" -f $info.Vid, $info.Pid, $info.Version)
        Write-Line "  product  : $($info.Product)"
        Write-Line ("  usage    : page 0x{0:X2} usage 0x{1:X2}  ({2})" -f `
                    $info.UsagePage, $info.Usage, (Get-UsageName $info.UsagePage $info.Usage))
        Write-Line "  reports  : input $($info.InputLen) B, output $($info.OutputLen) B, feature $($info.FeatureLen) B"
        Write-Line "  caps     : $($info.LinkCollections) link collections, $($info.InputButtonCaps) input button caps, $($info.InputValueCaps) input value caps"
    }
}

function Get-UsageName([int]$page, [int]$usage)
{
    if ($page -ne 0x01) { return "usage page 0x{0:X2}" -f $page }
    switch ($usage)
    {
        0x02    { 'Mouse' }
        0x04    { 'Joystick' }
        0x05    { 'Game Pad' }
        0x06    { 'Keyboard' }
        0x07    { 'Keypad' }
        default { "Generic Desktop usage 0x{0:X2}" -f $usage }
    }
}

# --- live mode --------------------------------------------------------------
function Show-Live
{
    Write-Line "Capturing Raw Input for $Seconds s, device filter '$Match'..."
    Write-Line "(reports from other keyboards and mice are counted but not shown)"

    $tap = [RawInputTap]::Run($Match, $Seconds)

    Write-Line ''
    foreach ($e in $tap.Reports) { Write-Line $e }

    Write-Line ''
    Write-Line "$($tap.Reports.Count) report(s) from '$Match', $($tap.Skipped) from other devices."
}

# --- main -------------------------------------------------------------------
Write-Line "hid-sniffer  $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"

if ($Live) { Show-Live } else { Show-Descriptor }
