import sedona.Slot;import sedona.Value;
/** Strict scalar parsing shared by prepare and unit checks. No coercion/wrapping. */
final class PropertyWrite {
 static Value parse(Slot slot,String raw){
  if(!slot.isProp()||slot.name.equals("meta")||slot.isAsStr()||slot.facets.getb("readonly",false)||DeviceGateway.sensitive(slot.name))throw new IllegalArgumentException("Property is read only or unsupported");
  String s=raw.trim(),t=slot.type.name;Value v;
  switch(t){
   case "bool":if(!s.equals("true")&&!s.equals("false"))throw new IllegalArgumentException("Expected true or false");break;
   case "byte":case "short":case "int":case "long":
    if(!s.matches("-?[0-9]+"))throw new IllegalArgumentException("Expected integer");long n=Long.parseLong(s);long lo=t.equals("byte")||t.equals("short")?0:t.equals("int")?Integer.MIN_VALUE:Long.MIN_VALUE,hi=t.equals("byte")?255:t.equals("short")?65535:t.equals("int")?Integer.MAX_VALUE:Long.MAX_VALUE;if(n<lo||n>hi)throw new IllegalArgumentException("Integer out of range");break;
   case "float":case "double":double number=Double.parseDouble(s);if(!Double.isFinite(number)||t.equals("float")&&!Float.isFinite((float)number))throw new IllegalArgumentException("Expected finite number in type range");break;
   default:throw new IllegalArgumentException("Only scalar numeric/Boolean properties are supported");
  }
  v=Value.defaultForType(slot.type.id).decodeString(s);slot.assertValue(v);return v;
 }
}

