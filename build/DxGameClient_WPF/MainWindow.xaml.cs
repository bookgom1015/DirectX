using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

using DxGameClient_WPF.Models;
using DxGameClient_WPF.ModelViews;
using DxGameClient_WPF.Views;

namespace DxGameClient_WPF {
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window {
        public MainWindow() {
            InitializeComponent();

            ViewModel = new MainWindowViewModel();
            DataContext = ViewModel;

            ViewModel.Initialize();
            ViewModel.SetOnNotifyIconDoubleClicked(delegate (object sender, EventArgs eventArgs) {
                Visibility = Visibility.Visible;
            });
            ViewModel.AddNotifyIconMenuItem("Show/Hide", delegate (object sender, EventArgs eventArgs) {
                if (Visibility == Visibility.Visible)
                    Visibility = Visibility.Hidden;
                else if (Visibility == Visibility.Hidden)
                    Visibility = Visibility.Visible;
            });
            ViewModel.AddNotifyIconMenuItem("Close", delegate (object sender, EventArgs eventArgs) {
                Close();
            });

            Pamphlet mainPamphlet = new Pamphlet { ImageFilePath = "/Assets/Images/yor_600x750.png", Title = "Title", Desc = "Descriptions" };
            MainFrame.NavigationService.Navigate(new PamphletPage(mainPamphlet));

            Pamphlet leftSubPamphlet = new Pamphlet { ImageFilePath = "/Assets/Images/room_600x1067.png", Title = "Title", Desc = "Descriptions" };
            LeftSubFrame.NavigationService.Navigate(new PamphletPage(leftSubPamphlet));

            Pamphlet rightSubPamphlet = new Pamphlet { ImageFilePath = "/Assets/Images/libarary_600x1067.png", Title = "Title", Desc = "Descriptions" };
            RightSubFrame.NavigationService.Navigate(new PamphletPage(rightSubPamphlet));
        }

        protected override void OnClosing(CancelEventArgs e) {
            base.OnClosing(e);

            ViewModel.CleanUp();
        }

        private void HeaderThumb_DragDelta(object sender, System.Windows.Controls.Primitives.DragDeltaEventArgs e) {
            Left = Left + e.HorizontalChange;
            Top = Top + e.VerticalChange;
        }

        private void CloseButton_Click(object sender, RoutedEventArgs e) {
            Visibility = Visibility.Hidden;
        }

        private void PlayButton_Click(object sender, RoutedEventArgs e) {
            String currentDirectory = System.AppDomain.CurrentDomain.BaseDirectory;
            int pos = currentDirectory.LastIndexOf("DirectX\\bin");
            pos = currentDirectory.IndexOf('\\', pos + 11);

            String binPath = currentDirectory.Substring(0, pos);
            String filePath = binPath + "\\x64\\DX12Game\\Release\\DX12Game.exe";
            
            if (!File.Exists(filePath)) {
                MessageBox.Show("Missing file");
                return;
            }

            try {
                ProcessStartInfo startInfo = new ProcessStartInfo();
                startInfo.FileName = filePath;
                startInfo.WorkingDirectory = binPath + "\\x64\\DX12Game\\Release";
                Process.Start(startInfo);
            }
            catch (Exception except) {
                MessageBox.Show(except.Message);
            }

            Visibility = Visibility.Hidden;
        }

        private void ConfigureButton_Click(object sender, RoutedEventArgs e) {

        }

        private MainWindowViewModel ViewModel;
    }
}
