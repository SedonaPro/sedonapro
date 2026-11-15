package sedona.sox;
/** Forgets the stock client's cached components: an action may replace components under the same ids on the device ("Component has changed type!"). Split-package helper like SchemaParts. */
public final class ClientCache {
 private ClientCache(){}
 public static void forget(SoxClient client){synchronized(client){if(client.cache!=null)java.util.Arrays.fill(client.cache,null);}}
}
