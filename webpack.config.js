const path = require('path');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const CompressionPlugin = require('compression-webpack-plugin');

module.exports = {
  entry: './archiv/aktuell/index.js',
  output: {
    filename: 'index.js',
    path: path.resolve(__dirname, 'data/www'),
  },
  mode: 'production',
  module: {
    rules: [
      {
        test: /\.css$/i,
        use: ['style-loader', 'css-loader'],
      },
    ],
  },
  plugins: [
    new HtmlWebpackPlugin({
      template: './archiv/aktuell/index.html',  // Your HTML file
      filename: 'index.html',  // The output HTML file
      favicon: './archiv/aktuell/favicon.png',  // Add this line to include the faviconnp
    }),
    new CompressionPlugin({
      filename: '[path][base].gz',
      algorithm: 'gzip',
      test: /\.js$|\.css$|\.html$/,
      threshold: 0,
      minRatio: 0.8,
    }),
  ],
};
