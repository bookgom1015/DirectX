using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Forms;

using DxGameClient_WPF.Models;

namespace DxGameClient_WPF.ModelViews {
    public class MainWindowViewModel : INotifyPropertyChanged {
        public MainWindowViewModel() {
            Pamphlets = new List<Pamphlet>();
        }

        protected virtual void OnPropertyChanged([CallerMemberName] string propertyName = null) {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        public void Initialize() {
            CreateNotifyIcon();
        }

        public void CleanUp() {
            _notifyIcon.Dispose();
        }

        public void SetOnNotifyIconDoubleClicked(EventHandler handler) {
            _notifyIcon.DoubleClick += handler;
        }

        public void AddNotifyIconMenuItem(string text, EventHandler handler) {
            MenuItem menuItem = new MenuItem();
            menuItem.Text = text;
            menuItem.Click += handler;

            _contextMenu.MenuItems.Add(menuItem);
        }

        private void CreateNotifyIcon() {
            string currDir = System.AppDomain.CurrentDomain.BaseDirectory;
            int pos = currDir.LastIndexOf("bin\\");

            String solutionPath = currDir.Substring(0, pos);
            String iconPath = solutionPath + "\\build\\DxGameClient_WPF\\Assets\\Icons\\chipmunk.ico";

            _notifyIcon = new NotifyIcon();
            _notifyIcon.Icon = new System.Drawing.Icon(iconPath);
            _notifyIcon.Visible = true;
            _notifyIcon.Text = "DX12Game Client";

            _contextMenu = new ContextMenu();
            _notifyIcon.ContextMenu = _contextMenu;
        }

        public event PropertyChangedEventHandler PropertyChanged;

        private List<Pamphlet> _pamphlets;
        public List<Pamphlet> Pamphlets {
            get { return _pamphlets; }
            set { _pamphlets = value; OnPropertyChanged("Pamphlets"); }
        }

        private NotifyIcon _notifyIcon;
        private ContextMenu _contextMenu;

        private bool _firstClose = true;
        public bool FirstClose {
            get => _firstClose;
            set => _firstClose = value;
        }
    }
}
