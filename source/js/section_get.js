(f=function(){
var selection=window.getSelection();
if(selection.rangeCount===0)return;
var range=selection.getRangeAt(0);
var root=range.commonAncestorContainer;
var start=range.startContainer;
var end=range.endContainer;
while (true) {
 if(root===null)return;
 tag=root.tagName;
 if(tag==="BODY")return;
 if(tag==="FB:BODY"||type==="FB:SECTION")break;
 root = root.parentNode;
}
while(start.parentNode!==root) {
 if(start===null)return;
 start=start.parentNode;
}
while(end.parentNode!==root) {
 if(end===null)return;
 end=end.parentNode;
}
return location(root)
+"|"+$(root).children().index(start)
+","+$(root).children().index(end)
+"|"+locator(range.startContainer)
+","+range.startOffset
+","+locator(range.endContainer)
+","+range.endOffset;
})()
