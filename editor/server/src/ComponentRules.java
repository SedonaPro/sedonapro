import java.util.*;
/**
 * Graphic components of UI format v3: a small picture (symbols and widgets in per cent of its box)
 * whose values are named points; a part placed on a view binds slots to those points. Same rules as
 * web/src/components.js and Sedona Studio.
 */
final class ComponentRules {
 static void require(boolean b,String message){if(!b)throw new IllegalArgumentException(message);}
 static boolean slot(Object x,int max){return x instanceof String&&!((String)x).isEmpty()&&((String)x).length()<=max&&!x.equals("meta")&&!((String)x).matches("(?i).*(password|secret|cred).*|(?i)key");}
 static boolean accepts(String kind,String type){String t=type.replace("sys::","");return kind.equals("bool")?t.equals("bool"):kind.equals("number")?Set.of("float","double","int","long","short","byte").contains(t):true;}
 /** Returns component id -> (point id -> kind). */
 static Map<String,Map<String,String>> components(Object input,Set<String> assets){
  require(input instanceof List&&((List<?>)input).size()<=64,"Maximum 64 graphic components");Map<String,Map<String,String>> out=new HashMap<>();
  for(Object entry:(List<?>)input){require(entry instanceof Map,"Invalid graphic component");Map<?,?> c=(Map<?,?>)entry;
   require(Set.of("id","title","width","height","points","objects","widgets").containsAll(c.keySet())&&c.keySet().containsAll(Set.of("id","title","width","height","points"))&&!out.containsKey(ViewStore.id(c.get("id")))&&SvgLibraryRules.text(c.get("title"),128),"Invalid graphic component fields");
   for(String key:List.of("width","height")){double n=SvgLibraryRules.number(c.get(key));require(n>0&&n<=2000,"Component size out of range");}
   require(c.get("points") instanceof List&&((List<?>)c.get("points")).size()<=16,"Maximum 16 points per component");Map<String,String> points=new HashMap<>();
   for(Object item:(List<?>)c.get("points")){require(item instanceof Map,"Invalid component point");Map<?,?> p=(Map<?,?>)item;
    require(p.keySet().equals(Set.of("id","label","kind","slots"))&&slot(ViewStore.id(p.get("id")),64)&&!points.containsKey(p.get("id"))&&SvgLibraryRules.text(p.get("label"),128)&&Set.of("bool","number","any").contains(p.get("kind"))&&p.get("slots") instanceof List&&((List<?>)p.get("slots")).size()<=8,"Invalid component point fields");
    for(Object name:(List<?>)p.get("slots"))require(slot(name,128),"Invalid suggested slot");points.put((String)p.get("id"),(String)p.get("kind"));}
   if(c.containsKey("objects")){require(c.get("objects") instanceof List&&((List<?>)c.get("objects")).size()<=16,"Maximum 16 pictures per component");SvgLibraryRules.objects(c.get("objects"),assets);for(Object o:(List<?>)c.get("objects"))for(String key:List.of("binding","fault"))if(((Map<?,?>)o).containsKey(key))inside(((Map<?,?>)o).get(key),points);}
   if(c.containsKey("widgets")){require(c.get("widgets") instanceof List&&((List<?>)c.get("widgets")).size()<=16,"Maximum 16 widgets per component");ViewStore.validateWidgets(c.get("widgets"));for(Object w:(List<?>)c.get("widgets")){Map<?,?> widget=(Map<?,?>)w;require(!Set.of("trend","bars","table").contains(widget.get("kind")),"Charts are not allowed in a component");if(!"text".equals(widget.get("kind")))inside(widget,points);}}
   out.put((String)c.get("id"),points);}
  return out;
 }
 static void inside(Object binding,Map<String,String> points){Map<?,?> b=(Map<?,?>)binding;require(".".equals(b.get("path"))&&points.containsKey(b.get("slot")),"A binding inside a component must name its point");}
 static void parts(Object input,Map<String,Map<String,String>> components){
  require(input instanceof List&&((List<?>)input).size()<=64,"Maximum 64 components per view");Set<String> ids=new HashSet<>();
  for(Object item:(List<?>)input){require(item instanceof Map,"Invalid placed component");Map<?,?> p=(Map<?,?>)item;
   require(p.keySet().equals(Set.of("id","componentId","label","x","y","bind"))&&ids.add(ViewStore.id(p.get("id")))&&components.containsKey(p.get("componentId"))&&SvgLibraryRules.text(p.get("label"),128)&&p.get("bind") instanceof Map,"Invalid placed component fields");
   for(String key:List.of("x","y")){double n=SvgLibraryRules.number(p.get(key));require(n>=0&&n<=100,"Component position out of range");}
   Map<String,String> points=components.get(p.get("componentId"));
   for(Map.Entry<?,?> e:((Map<?,?>)p.get("bind")).entrySet()){require(points.containsKey(e.getKey())&&e.getValue() instanceof Map,"Unknown component point");Map<?,?> b=(Map<?,?>)e.getValue();
    require(b.keySet().equals(Set.of("path","slot","type"))&&SvgLibraryRules.text(b.get("path"),2048)&&slot(b.get("slot"),128)&&SvgLibraryRules.text(b.get("type"),160)&&!((String)b.get("type")).isEmpty(),"Invalid point binding");
    String path=(String)b.get("path");require(!path.isEmpty()&&Arrays.stream(path.split("/",-1)).noneMatch(x->x.isEmpty()||x.equals("..")),"Unsafe point binding");
    require(accepts(points.get(e.getKey()),(String)b.get("type")),"Point binding type mismatch");}}
 }
}
