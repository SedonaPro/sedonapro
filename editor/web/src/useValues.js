import {useSyncExternalStore} from 'react';import{values}from'./model';
export function useValues(id){return useSyncExternalStore(fn=>values.subscribe(id,fn),()=>values.get(id));}
