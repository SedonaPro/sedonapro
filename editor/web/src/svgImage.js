import{safeSvg}from'./uiDocument';
export const SVG_MIME='application/x-mcs-svg-asset';
export function imageUri(svg,freeze=false){let clean=safeSvg(svg);if(freeze){const doc=new DOMParser().parseFromString(clean,'image/svg+xml');for(const n of doc.querySelectorAll('animateTransform'))n.remove();clean=new XMLSerializer().serializeToString(doc.documentElement);}return 'data:image/svg+xml;charset=utf-8,'+encodeURIComponent(clean);}
