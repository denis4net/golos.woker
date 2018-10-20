const EOSTest = require("eosio.test");

const eosTest = new EOSTest();
const appName = "app.sample";
const tokenSymbol = "APP";

const delegateAccounts = [];
const memberAccounts = [];

for (let i = 1; i < 22; i++) {
  const code = String.fromCharCode('a'.charCodeAt(0) + i)
  delegateAccounts.push(`delegate.${code}`)
  memberAccounts.push(`user.${code}`)
}

let tokenContract = null;
let contract = null;

beforeAll(async done => {
  console.log("init");
  await eosTest.init();

  await eosTest.newAccount(
    ...memberAccounts,
    ...delegateAccounts,
    "golos.worker",
    "golos.token",
    appName
  );

  console.log("build & deploy token contract");
  await eosTest.make("../golos.token");
  tokenContract = await eosTest.deploy(
    "golos.token",
    "../golos.token/golos.token.wasm",
    "../golos.token/golos.token.abi"
  );

  console.log("build contract");
  await eosTest.make(".");

  console.log("create app token");
  await tokenContract.create(appName, `1000000000 ${tokenSymbol}`, {
    authorization: "golos.token"
  });
  await tokenContract.issue(appName, `1000000000 ${tokenSymbol}`, {
    authorization: appName
  });

  console.log("deploy golos.worker contract");
  contract = await eosTest.deploy(
    "golos.worker",
    "golos.worker.wasm",
    "golos.worker.abi"
  );

  done();
}, 120000);

it(
  "1st use case test",
  async done => {
    console.log("create a workers pool");
    await contract.createpool(appName, tokenSymbol, { authorization: appName });

    console.log("Workers pool replenishment");
    await tokenContract.transfer(
      appName,
      "golos.worker",
      `1000 ${tokenSymbol}`,
      appName /* memo */
    );
    await tokenContract.transfer(
      appName,
      "golos.worker",
      `500 ${tokenSymbol}`,
      appName /* memo */
    );
    let funds = await eosTest.api.getTableRows(
      true,
      "golos.worker",
      appName,
      "funds",
      appName
    );
    console.log("funds table:", funds);
    expect(funds.rows.length).not.toEqual(0);
    expect(funds.rows[0].quantity).toEqual(`1500 ${tokenSymbol}`);

    console.log("addpropos");
    const proposals = [
      {
        id: 0,
        title: "Proposal #1",
        text: "Let's create worker's pool",
        user: memberAccounts[0]
      },
      {
        id: 1,
        title: "Proposal #2",
        text: "Let's create golos fork",
        user: memberAccounts[1]
      }
    ];

    const comments = [
      { id: 0, user: delegateAccounts[0], text: "Let's do it!" },
      { id: 1, user: delegateAccounts[1], text: "Noooo!" }
    ];

    const tspecs = [
      {
        id: 0,
        author: memberAccounts[2],
        text: "Technical specification #1",
        specification_cost: `100 ${tokenSymbol}`,
        specification_eta: Math.round(Date.now() / 1000 + 1000),
        development_cost: `200.0 ${tokenSymbol}`,
        development_eta: Math.round(Date.now() / 2000 + 30000),
        payments_count: 1
      },
      {
        id: 1,
        author: memberAccounts[3],
        text: "Technical specification #2",
        specification_cost: `500.0 ${tokenSymbol}`,
        specification_eta: Math.round(Date.now() / 1000 + 1000),
        development_cost: `900.0 ${tokenSymbol}`,
        development_eta: Math.round(Date.now() / 2000 + 30000),
        payments_count: 2
      }
    ];

    for (let proposal of proposals) {
      console.log(proposal);
      await contract.addpropos(
        appName,
        proposal.id,
        proposal.user,
        proposal.title,
        proposal.text,
        { authorization: proposal.user }
      );
      await contract.votepropos(appName, proposal.id, delegateAccounts[0], 1, {
        authorization: delegateAccounts[0]
      });
      await contract.votepropos(appName, proposal.id, delegateAccounts[1], 0, {
        authorization: delegateAccounts[1]
      });

      for (let comment of comments) {
        console.log("addcomment", comment);
        await contract.addcomment(
          appName,
          proposal.id,
          comment.id,
          comment.user,
          comment,
          { authorization: comment.user }
        );
      }

      for (let tspec of tspecs) {
        console.log("technical specification", tspec);
        await contract.addtspec(
          appName,
          proposal.id,
          tspec.id,
          tspec.author,
          tspec,
          { authorization: tspec.author }
        );
      }
    }

    console.log("getting proposals table");

    console.log(
      "proposals table",
      await eosTest.api.getTableRows(true, "golos.worker", appName, "proposals")
    );

    // for (let proposal of proposals) {
    //   for (let comment of comments) {
    //     console.log('delcomment', proposal, comment)
    //     await contract.delcomment(appName, proposal.id, comment.id, {authorization: comment.user})
    //   }

    //   await contract.delpropos(appName, proposal.id, {authorization: proposal.user})
    // }

    eosTest.destroy();
    done();
  },
  300000
);

afterAll(async done => {
  await eosTest.destroy();
  done();
});
