var os = require('os');
function getMyIP(){
  var version = 'IPv4';
  var internal = false
  var interfaces = os.networkInterfaces();
  for(var key in interfaces){
    var addresses = interfaces[key];
    for(var i = 0; i < addresses.length; i++){
      var address = addresses[i];
      if(address.internal === false && address.family === 'IPv4'){
        return address.address;
      };
    };
  };
  return 'localhost'
};
console.log(getMyIP());

