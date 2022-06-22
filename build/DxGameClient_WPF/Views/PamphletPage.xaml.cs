using System;
using System.Collections.Generic;
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

namespace DxGameClient_WPF.Views {
    /// <summary>
    /// Interaction logic for PamphletPage.xaml
    /// </summary>
    public partial class PamphletPage : Page {
        public PamphletPage(Pamphlet pamphlet) {
            InitializeComponent();

            MainImage.Source = new BitmapImage(new Uri(pamphlet.ImageFilePath, UriKind.Relative));
            TitleBlock.Text = pamphlet.Title;
            DescBlock.Text = pamphlet.Desc;
        }
    }
}
