#!/bin/sh

if [ $# -eq 0 ] ; then
  echo "Usage: $0 sdkconfig_path"
  exit
fi

if [ ! -f $1 ] ; then
  echo "Error : sdkconfig file not exist"
fi

. $1

if [ -z $CONFIG_WEBAPI_SERVER ] ; then
  echo "Error : valiable CONFIG_WEBAPI_SERVER not found."
  exit
fi

if [ -z $CONFIG_WEBAPI_PORT ] ; then
  echo "Error : valiable CONFIG_WEBAPI_PORT not found."
  exit
fi

(
  BEGIN_CER="-----BEGIN CERTIFICATE-----" 
  END_CER="-----END CERTIFICATE-----"
  echo  $BEGIN_CER
  openssl s_client -showcerts -connect $CONFIG_WEBAPI_SERVER:$CONFIG_WEBAPI_PORT < /dev/null 2>/dev/null | \
    sed -n "/$BEGIN_CER/,/$END_CER/p"   | \
    awk 'BEGIN{RS="-----"} NR==7{print $0}' | \
    sed  '/^$/d' 
  echo $END_CER 
) 
