Wallet
------

- Add `-coinjoinfreshchange` option to control change destination behavior
  during CoinJoin denomination creation. By default (flag unset), change is
  sent back to the source address (legacy behavior). When enabled, change is
  sent to a fresh change address to avoid address/public key reuse. (#6870)


