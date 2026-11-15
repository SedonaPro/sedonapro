import java.util.*;
/** Monotonic leases. No browser event is trusted as the sole cleanup mechanism. */
final class InterestRegistry {
 static final long TTL=8_000_000_000L;
 static final class Lease{Set<Integer> ids=new HashSet<>();long deadline;}
 private final Map<String,Lease> leases=new HashMap<>();
 String create(long now){if(leases.size()>=8)throw new IllegalStateException("Maximum 8 viewers");String id=UUID.randomUUID().toString();Lease l=new Lease();l.deadline=now+TTL;leases.put(id,l);return id;}
 void renew(String id,Collection<Integer> ids,long now){Lease l=leases.get(id);if(l==null||l.deadline<=now)throw new IllegalArgumentException("Viewer lease expired; connect again");if(ids.size()>128)throw new IllegalArgumentException("Maximum 128 watched components");l.ids=new HashSet<>(ids);l.deadline=now+TTL;}
 void release(String id){leases.remove(id);}void clear(){leases.clear();}
 boolean touch(String id,long now){Lease l=leases.get(id);if(l==null||l.deadline<=now)return false;l.deadline=now+TTL;return true;}
 boolean contains(String id){return leases.containsKey(id);}int size(){return leases.size();}
 void expire(long now){leases.values().removeIf(l->l.deadline<=now);}
 Set<Integer> union(){Set<Integer> ids=new HashSet<>();for(Lease l:leases.values())ids.addAll(l.ids);return ids;}
}
