using System;

using DXGameClient.ViewModels;

using Windows.ApplicationModel.Core;
using Windows.Foundation;
using Windows.UI;
using Windows.UI.ViewManagement;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media.Animation;

namespace DXGameClient.Views {
    public sealed partial class MainPage : Page {
        public MainViewModel ViewModel { get; } = new MainViewModel();

        public MainPage() {
            InitializeComponent();

            ApplicationView.GetForCurrentView().SetPreferredMinSize(new Size(500, 500));

            SetTitleBar();
        }

        public void CustomTitleBarLayoutMetricsChanged(CoreApplicationViewTitleBar sender, object args) {
            CoreApplicationViewTitleBar titleBar = sender as CoreApplicationViewTitleBar;
            if (titleBar == null) return;
        }

        public void SetTitleBar() {
            CoreApplicationViewTitleBar coreTitleBar = CoreApplication.GetCurrentView().TitleBar;
            coreTitleBar.LayoutMetricsChanged += CustomTitleBarLayoutMetricsChanged;
            coreTitleBar.ExtendViewIntoTitleBar = true;

            Window.Current.SetTitleBar(UserLayout);

            ApplicationViewTitleBar titleBar = ApplicationView.GetForCurrentView().TitleBar;
            titleBar.ButtonForegroundColor = Colors.Black;
            titleBar.ButtonBackgroundColor = Colors.Transparent;
            titleBar.ButtonInactiveBackgroundColor = Colors.Transparent;
        }
    }
}
