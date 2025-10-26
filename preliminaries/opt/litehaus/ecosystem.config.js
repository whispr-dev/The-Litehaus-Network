// /opt/litehaus/ecosystem.config.js
module.exports = {
  apps: [
    {
      name: "litehaus-beacon",
      script: "/opt/litehaus/server.js",
      env: {
        NODE_ENV: "production"
      }
    },
    {
      name: "litehaus-listener",
      script: "/opt/litehaus/litehaus-listener/server.js",
      env: {
        NODE_ENV: "production"
      }
    }
  ]
};
