// -=( some usefull/weird staff )=----------------------------
//var Module;
function goto(url, ok, fail) {
   console.log('trying to go to ' + url);
   var address = url[0];
   for (var i = 1; i < url.length; i++) {
      address += '/';
      var arg = url[i];
      if (typeof arg === 'string' || arg instanceof String)
         address += "'" + escape(arg);
      else
         address += arg;
   }

   //was: document.location.href = address;
   console.log(address);
   $.ajax({
      url: address,
      context: document.body
   }).done(function(text) {
      console.log(address + " ok.");
      console.log("trying to call " + '('+ok+' '+ text +')');
      Module.send('('+ok+' '+ text +')\n');
      console.log("--ok");
      //(window.Module || window.localStorage.Ol).send('('+ok+' '+ text +')\n');
   }).error(function(text) {
      console.log(address + " failed.");
      console.log(text);
      console.log("trying to call " + '('+fail+' '+ text.status +')');
      Module.send('('+fail+' '+ text.status +')\n');
      console.log("--fail");
      //(window.Module || window.localStorage.Ol).send('('+fail+' '+ text.status +')\n');
   });

   console.log("fyi: false");
   return false;
}

function redirect() {
	setTimeout(function(){goto($('#redirect').attr('href'))}, 2000);
}
