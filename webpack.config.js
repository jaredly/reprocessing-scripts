const path = require('path');

module.exports = {
  entry: './lib/js/src/web.js',
  output: {
    path: path.join(__dirname, "public", "assets"),
    filename: 'bundle.js',
  },
};

